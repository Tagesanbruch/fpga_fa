#include "fa_task_queue_runtime.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(FA_RUNTIME_ENABLE_XRT)
#if __has_include(<xrt/xrt_bo.h>)
#include <xrt/xrt_bo.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_uuid.h>
#include <xrt/xrt_xclbin.h>
#elif __has_include(<experimental/xrt_bo.h>)
#include <experimental/xrt_bo.h>
#include <experimental/xrt_device.h>
#include <experimental/xrt_kernel.h>
#include <experimental/xrt_xclbin.h>
#else
#error "FA_RUNTIME_ENABLE_XRT was set but XRT headers were not found"
#endif
#endif

namespace fpga::llama {

namespace {

constexpr int kDefaultStrideBytes = fpga::fa::kHeadDim * static_cast<int>(sizeof(int16_t));

bool validate_task(const AttentionTask &task, std::string *error) {
  if (!task.q || !task.k || !task.v || !task.o) {
    if (error) {
      *error = "task pointers must not be null";
    }
    return false;
  }
  if (task.seq_len < 1 || task.seq_len > fpga::fa::kMaxSeqLen) {
    if (error) {
      *error = "task seq_len must be in [1, 256]";
    }
    return false;
  }
  if ((task.seq_len % fpga::fa::kTileQ) != 0 || (task.seq_len % fpga::fa::kTileK) != 0) {
    if (error) {
      *error = "task seq_len must be a multiple of both 32 and 64 for the current HLS kernel";
    }
    return false;
  }
  if (task.stride_bytes != kDefaultStrideBytes) {
    if (error) {
      *error = "task stride_bytes must match the dense D=64 Q8.8 layout";
    }
    return false;
  }
  return true;
}

void run_software_task(const RuntimeOptions &options, const AttentionTask &task) {
  uint32_t local_profile[fpga::fa::kProfileWords] = {};
  uint32_t *profile = task.profile ? task.profile : local_profile;
  fpga::fa::clear_profile(profile);
  switch (options.mode) {
    case RuntimeMode::kSoftwareHls:
      fpga::fa::run_attention_tiled_hls(task.q, task.k, task.v, task.o, task.seq_len, task.stride_bytes,
                                        task.scale_q8_8, task.neg_large_q8_8, task.causal, profile);
      break;
    case RuntimeMode::kStrictQ8_8:
      fpga::fa::run_attention_strict(task.q, task.k, task.v, task.o, task.seq_len, task.stride_bytes,
                                     task.scale_q8_8, task.neg_large_q8_8, task.causal, profile);
      break;
    case RuntimeMode::kXrt:
      throw std::runtime_error("XRT mode requires a hardware-backed runtime");
  }
}

}  // namespace

struct AttentionTaskQueueRuntime::Impl {
  bool initialized = false;
  std::vector<AttentionTask> queue;

#if defined(FA_RUNTIME_ENABLE_XRT)
  std::unique_ptr<xrt::device> device;
  std::unique_ptr<xrt::xclbin> xclbin;
  xrt::uuid uuid{};
  std::unique_ptr<xrt::kernel> kernel;
  std::unique_ptr<xrt::bo> q_bo;
  std::unique_ptr<xrt::bo> k_bo;
  std::unique_ptr<xrt::bo> v_bo;
  std::unique_ptr<xrt::bo> o_bo;
  std::size_t capacity_bytes = 0;

  void ensure_buffers(std::size_t matrix_bytes) {
    if (capacity_bytes >= matrix_bytes && q_bo && k_bo && v_bo && o_bo) {
      return;
    }

    q_bo = std::make_unique<xrt::bo>(*device, matrix_bytes, kernel->group_id(0));
    k_bo = std::make_unique<xrt::bo>(*device, matrix_bytes, kernel->group_id(1));
    v_bo = std::make_unique<xrt::bo>(*device, matrix_bytes, kernel->group_id(2));
    o_bo = std::make_unique<xrt::bo>(*device, matrix_bytes, kernel->group_id(3));
    capacity_bytes = matrix_bytes;
  }

  void run_xrt_task(const AttentionTask &task) {
    const std::size_t matrix_bytes = static_cast<std::size_t>(task.seq_len * task.stride_bytes);
    ensure_buffers(matrix_bytes);

    std::memcpy(q_bo->map<void *>(), task.q, matrix_bytes);
    std::memcpy(k_bo->map<void *>(), task.k, matrix_bytes);
    std::memcpy(v_bo->map<void *>(), task.v, matrix_bytes);
    std::memset(o_bo->map<void *>(), 0, matrix_bytes);

    q_bo->sync(XCL_BO_SYNC_BO_TO_DEVICE);
    k_bo->sync(XCL_BO_SYNC_BO_TO_DEVICE);
    v_bo->sync(XCL_BO_SYNC_BO_TO_DEVICE);

    auto run = (*kernel)(*q_bo, *k_bo, *v_bo, *o_bo, task.seq_len, task.stride_bytes, task.scale_q8_8,
                         task.neg_large_q8_8, task.causal ? 1 : 0, static_cast<uint64_t>(0));
    run.wait();

    o_bo->sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    std::memcpy(task.o, o_bo->map<void *>(), matrix_bytes);
    if (task.profile) {
      fpga::fa::clear_profile(task.profile);
    }
  }
#endif
};

AttentionTaskQueueRuntime::AttentionTaskQueueRuntime(RuntimeOptions options) : options_(std::move(options)), impl_(new Impl()) {
  impl_->queue.reserve(std::max<std::size_t>(1, options_.queue_depth));
}

AttentionTaskQueueRuntime::~AttentionTaskQueueRuntime() {
  delete impl_;
}

bool AttentionTaskQueueRuntime::init(std::string *error) {
  if (impl_->initialized) {
    return true;
  }

  try {
#if defined(FA_RUNTIME_ENABLE_XRT)
    if (options_.mode == RuntimeMode::kXrt) {
      impl_->device = std::make_unique<xrt::device>(options_.device_index);
      impl_->xclbin = std::make_unique<xrt::xclbin>(options_.xclbin_path);
      impl_->uuid = impl_->device->load_xclbin(*impl_->xclbin);
      impl_->kernel = std::make_unique<xrt::kernel>(*impl_->device, impl_->uuid, options_.kernel_name);
    }
#else
    if (options_.mode == RuntimeMode::kXrt) {
      if (error) {
        *error = "runtime was built without XRT support";
      }
      return false;
    }
#endif

    impl_->initialized = true;
    return true;
  } catch (const std::exception &ex) {
    if (error) {
      *error = ex.what();
    }
    return false;
  }
}

bool AttentionTaskQueueRuntime::ready() const {
  return impl_->initialized;
}

std::size_t AttentionTaskQueueRuntime::queue_depth() const {
  return std::max<std::size_t>(1, options_.queue_depth);
}

std::size_t AttentionTaskQueueRuntime::pending_tasks() const {
  return impl_->queue.size();
}

const RuntimeOptions &AttentionTaskQueueRuntime::options() const {
  return options_;
}

bool AttentionTaskQueueRuntime::submit(const AttentionTask &task, std::string *error) {
  if (!impl_->initialized && !init(error)) {
    return false;
  }
  if (!validate_task(task, error)) {
    return false;
  }
  impl_->queue.push_back(task);
  if (impl_->queue.size() >= queue_depth()) {
    return drain(error);
  }
  return true;
}

bool AttentionTaskQueueRuntime::drain(std::string *error) {
  if (!impl_->initialized && !init(error)) {
    return false;
  }

  try {
    for (const auto &task : impl_->queue) {
      switch (options_.mode) {
        case RuntimeMode::kSoftwareHls:
        case RuntimeMode::kStrictQ8_8:
          run_software_task(options_, task);
          break;
        case RuntimeMode::kXrt:
#if defined(FA_RUNTIME_ENABLE_XRT)
          impl_->run_xrt_task(task);
          break;
#else
          throw std::runtime_error("XRT mode requested but runtime was built without XRT support");
#endif
      }
    }
    impl_->queue.clear();
    return true;
  } catch (const std::exception &ex) {
    if (error) {
      *error = ex.what();
    }
    impl_->queue.clear();
    return false;
  }
}

void AttentionTaskQueueRuntime::clear() {
  impl_->queue.clear();
}

const char *runtime_mode_name(RuntimeMode mode) {
  switch (mode) {
    case RuntimeMode::kSoftwareHls:
      return "software_hls";
    case RuntimeMode::kStrictQ8_8:
      return "strict_q8_8";
    case RuntimeMode::kXrt:
      return "xrt";
  }
  return "unknown";
}

}  // namespace fpga::llama
