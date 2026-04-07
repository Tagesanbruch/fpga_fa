#pragma once

#include <cstdint>

namespace fpga {
namespace linear {

constexpr int kMaxInDim = 256;
constexpr int kMaxOutDim = 256;
constexpr int kMaxQueueTasks = 16;
constexpr int kTileOut = 32;
constexpr int kProfileWords = 16;

struct GemvTaskDesc {
  uint32_t x_offset_elems;
  uint32_t w_offset_elems;
  uint32_t y_offset_elems;
  uint16_t in_dim;
  uint16_t out_dim;
};

enum ProfileIndex : int {
  kProfileInDim = 0,
  kProfileOutDim = 1,
  kProfileWeightElems = 2,
  kProfileInputElems = 3,
  kProfileOutputElems = 4,
  kProfileMacs = 5,
  kProfileTileOutIters = 6,
  kProfileTaskCount = 7,
  kProfileTaskDescLoads = 8,
  kProfileQueueXElems = 9,
  kProfileQueueWElems = 10,
  kProfileQueueYElems = 11,
  kProfileMaxInDim = 12,
  kProfileMaxOutDim = 13,
  kProfileQueueKernelCalls = 14,
  kProfileReserved0 = 15,
};

void clear_profile(uint32_t *profile);

void run_gemv_strict(const int16_t *x,
                     const int16_t *w,
                     int16_t *y,
                     int in_dim,
                     int out_dim,
                     uint32_t *profile);

void run_gemv_tiled_hls(const int16_t *x,
                        const int16_t *w,
                        int16_t *y,
                        int in_dim,
                        int out_dim,
                        uint32_t *profile);

void run_gemv_queue_strict(const int16_t *x_all,
                           const int16_t *w_all,
                           int16_t *y_all,
                           const GemvTaskDesc *tasks,
                           int task_count,
                           uint32_t *profile);

void run_gemv_queue_tiled_hls(const int16_t *x_all,
                              const int16_t *w_all,
                              int16_t *y_all,
                              const GemvTaskDesc *tasks,
                              int task_count,
                              uint32_t *profile);

}  // namespace linear
}  // namespace fpga
