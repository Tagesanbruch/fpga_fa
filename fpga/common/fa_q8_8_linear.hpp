#pragma once

#include <cstdint>

namespace fpga {
namespace linear {

constexpr int kMaxInDim = 256;
constexpr int kMaxOutDim = 256;
constexpr int kTileOut = 32;
constexpr int kProfileWords = 8;

enum ProfileIndex : int {
  kProfileInDim = 0,
  kProfileOutDim = 1,
  kProfileWeightElems = 2,
  kProfileInputElems = 3,
  kProfileOutputElems = 4,
  kProfileMacs = 5,
  kProfileTileOutIters = 6,
  kProfileReserved0 = 7,
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

}  // namespace linear
}  // namespace fpga
