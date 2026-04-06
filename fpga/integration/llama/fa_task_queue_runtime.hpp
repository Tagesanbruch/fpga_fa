#pragma once

#include "../../common/fa_q8_8_attention.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fpga::llama {

enum class RuntimeMode {
  kSoftwareHls,
  kStrictQ8_8,
  kXrt,
};

struct RuntimeOptions {
  RuntimeMode mode = RuntimeMode::kSoftwareHls;
  std::string xclbin_path;
  std::string kernel_name = "fa_attention_kernel";
  int device_index = 0;
  std::size_t queue_depth = 8;
};

struct AttentionTask {
  const int16_t *q = nullptr;
  const int16_t *k = nullptr;
  const int16_t *v = nullptr;
  int16_t *o = nullptr;
  int seq_len = 0;
  int stride_bytes = fpga::fa::kHeadDim * static_cast<int>(sizeof(int16_t));
  int16_t scale_q8_8 = 32;
  int16_t neg_large_q8_8 = -2048;
  bool causal = true;
  uint32_t *profile = nullptr;
};

class AttentionTaskQueueRuntime {
 public:
  explicit AttentionTaskQueueRuntime(RuntimeOptions options = {});
  ~AttentionTaskQueueRuntime();

  AttentionTaskQueueRuntime(const AttentionTaskQueueRuntime &) = delete;
  AttentionTaskQueueRuntime &operator=(const AttentionTaskQueueRuntime &) = delete;

  bool init(std::string *error = nullptr);
  bool ready() const;

  std::size_t queue_depth() const;
  std::size_t pending_tasks() const;
  const RuntimeOptions &options() const;

  bool submit(const AttentionTask &task, std::string *error = nullptr);
  bool drain(std::string *error = nullptr);
  void clear();

 private:
  struct Impl;
  RuntimeOptions options_;
  Impl *impl_;
};

const char *runtime_mode_name(RuntimeMode mode);

}  // namespace fpga::llama
