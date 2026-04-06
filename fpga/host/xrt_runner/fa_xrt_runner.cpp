#include "../../common/fa_q8_8_attention.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

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
#error "XRT native C++ headers were not found. Set XILINX_XRT before building."
#endif

namespace {

struct Options {
  std::string xclbin_path;
  std::string kernel_name = "fa_attention_kernel";
  int device_index = 0;
  int seq_len = 64;
  int seed = 20260406;
  bool causal = true;
  bool verify = false;
};

void usage(const char *argv0) {
  std::cerr << "Usage: " << argv0
            << " --xclbin <file> [--kernel <name>] [--device <idx>] [--seq-len <1..256>] [--seed <n>]"
               " [--non-causal] [--verify]\n";
}

Options parse_args(int argc, char **argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--xclbin" && i + 1 < argc) {
      opt.xclbin_path = argv[++i];
    } else if (arg == "--kernel" && i + 1 < argc) {
      opt.kernel_name = argv[++i];
    } else if (arg == "--device" && i + 1 < argc) {
      opt.device_index = std::stoi(argv[++i]);
    } else if (arg == "--seq-len" && i + 1 < argc) {
      opt.seq_len = std::stoi(argv[++i]);
    } else if (arg == "--seed" && i + 1 < argc) {
      opt.seed = std::stoi(argv[++i]);
    } else if (arg == "--non-causal") {
      opt.causal = false;
    } else if (arg == "--verify") {
      opt.verify = true;
    } else {
      usage(argv[0]);
      throw std::runtime_error("invalid arguments");
    }
  }

  if (opt.xclbin_path.empty()) {
    usage(argv[0]);
    throw std::runtime_error("missing --xclbin");
  }
  if (opt.seq_len < 1 || opt.seq_len > fpga::fa::kMaxSeqLen) {
    throw std::runtime_error("seq_len must be in [1, 256]");
  }
  return opt;
}

std::vector<int16_t> make_random_matrix(int seq_len, int stride_elems, std::mt19937 &rng) {
  std::uniform_real_distribution<float> dist(-1.5f, 1.5f);
  std::vector<int16_t> data(static_cast<size_t>(seq_len * stride_elems), 0);
  for (int i = 0; i < seq_len; ++i) {
    for (int d = 0; d < fpga::fa::kHeadDim; ++d) {
      data[static_cast<size_t>(i * stride_elems + d)] =
          static_cast<int16_t>(std::lround(dist(rng) * 256.0f));
    }
  }
  return data;
}

bool verify_output(const std::vector<int16_t> &q,
                   const std::vector<int16_t> &k,
                   const std::vector<int16_t> &v,
                   const std::vector<int16_t> &got,
                   int seq_len,
                   int stride_bytes,
                   int16_t scale_q8_8,
                   int16_t neg_large_q8_8,
                   bool causal) {
  std::vector<int16_t> ref(got.size(), 0);
  uint32_t profile[fpga::fa::kProfileWords] = {};
  fpga::fa::run_attention_strict(q.data(), k.data(), v.data(), ref.data(), seq_len, stride_bytes, scale_q8_8,
                                 neg_large_q8_8, causal, profile);

  const int stride_elems = stride_bytes / static_cast<int>(sizeof(int16_t));
  for (int i = 0; i < seq_len; ++i) {
    for (int d = 0; d < fpga::fa::kHeadDim; ++d) {
      const auto idx = static_cast<size_t>(i * stride_elems + d);
      if (got[idx] != ref[idx]) {
        std::cerr << "[FAIL] mismatch at (" << i << "," << d << "): got=" << got[idx] << " ref=" << ref[idx]
                  << "\n";
        return false;
      }
    }
  }
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  try {
    const Options opt = parse_args(argc, argv);
    const int stride_bytes = fpga::fa::kHeadDim * static_cast<int>(sizeof(int16_t));
    const int stride_elems = stride_bytes / static_cast<int>(sizeof(int16_t));
    const int16_t scale_q8_8 = 32;
    const int16_t neg_large_q8_8 = -2048;

    std::mt19937 rng(opt.seed);
    auto q = make_random_matrix(opt.seq_len, stride_elems, rng);
    auto k = make_random_matrix(opt.seq_len, stride_elems, rng);
    auto v = make_random_matrix(opt.seq_len, stride_elems, rng);
    std::vector<int16_t> o(static_cast<size_t>(opt.seq_len * stride_elems), 0);
    std::vector<uint32_t> profile(fpga::fa::kProfileWords, 0);

    xrt::device device(opt.device_index);
    xrt::xclbin xclbin(opt.xclbin_path);
    auto uuid = device.load_xclbin(xclbin);
    xrt::kernel kernel(device, uuid, opt.kernel_name);

    const size_t matrix_bytes = static_cast<size_t>(opt.seq_len * stride_bytes);
    const size_t profile_bytes = static_cast<size_t>(fpga::fa::kProfileWords * sizeof(uint32_t));

    xrt::bo q_bo(device, matrix_bytes, kernel.group_id(0));
    xrt::bo k_bo(device, matrix_bytes, kernel.group_id(1));
    xrt::bo v_bo(device, matrix_bytes, kernel.group_id(2));
    xrt::bo o_bo(device, matrix_bytes, kernel.group_id(3));
    xrt::bo profile_bo(device, profile_bytes, kernel.group_id(9));

    std::memcpy(q_bo.map<void *>(), q.data(), matrix_bytes);
    std::memcpy(k_bo.map<void *>(), k.data(), matrix_bytes);
    std::memcpy(v_bo.map<void *>(), v.data(), matrix_bytes);
    std::memset(o_bo.map<void *>(), 0, matrix_bytes);
    std::memset(profile_bo.map<void *>(), 0, profile_bytes);

    q_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    k_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    v_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    profile_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    auto run = kernel(q_bo, k_bo, v_bo, o_bo, opt.seq_len, stride_bytes, scale_q8_8, neg_large_q8_8,
                      opt.causal ? 1 : 0, profile_bo);
    run.wait();

    o_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    profile_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    std::memcpy(o.data(), o_bo.map<void *>(), matrix_bytes);
    std::memcpy(profile.data(), profile_bo.map<void *>(), profile_bytes);

    std::cout << "[INFO] seq_len=" << opt.seq_len << " causal=" << (opt.causal ? 1 : 0)
              << " q_tiles=" << profile[fpga::fa::kProfileQTiles]
              << " k_tiles=" << profile[fpga::fa::kProfileKTiles]
              << " score_evals=" << profile[fpga::fa::kProfileScoreEvals] << "\n";

    if (opt.verify) {
      const bool ok =
          verify_output(q, k, v, o, opt.seq_len, stride_bytes, scale_q8_8, neg_large_q8_8, opt.causal);
      std::cout << (ok ? "[PASS] XRT output matches strict Q8.8 reference\n"
                       : "[FAIL] XRT output differs from strict Q8.8 reference\n");
      return ok ? 0 : 1;
    }

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "[ERR] " << e.what() << "\n";
    return 1;
  }
}
