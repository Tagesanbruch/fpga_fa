#include "fa_q8_8_linear.hpp"

#include <algorithm>
#include <cstddef>

namespace fpga {
namespace linear {

namespace {

int16_t sat_s16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

int16_t q8_8_round_shift(int64_t v) {
  const int64_t rounded = (v >= 0) ? (v + 128) : (v - 128);
  return sat_s16(static_cast<int32_t>(rounded >> 8));
}

void load_x_tile(const int16_t *x, int16_t x_local[kMaxInDim], int in_dim) {
  for (int i = 0; i < in_dim; ++i) {
#pragma HLS PIPELINE II=1
    x_local[i] = x[i];
  }
}

void compute_tile(const int16_t x_local[kMaxInDim],
                  const int16_t *w,
                  int16_t *y,
                  int in_dim,
                  int out_start,
                  int valid_out) {
  for (int oi = 0; oi < kTileOut; ++oi) {
    if (oi >= valid_out) {
      continue;
    }

    int64_t acc = 0;
    const int row = out_start + oi;
    const int row_base = row * in_dim;

    for (int i = 0; i < in_dim; ++i) {
#pragma HLS UNROLL factor=8
      const int32_t prod = static_cast<int32_t>(x_local[i]) * static_cast<int32_t>(w[row_base + i]);
#pragma HLS bind_op variable=prod op=mul impl=dsp
      acc += prod;
    }

    y[row] = q8_8_round_shift(acc);
  }
}

}  // namespace

void clear_profile(uint32_t *profile) {
  if (!profile) return;
  for (int i = 0; i < kProfileWords; ++i) {
    profile[i] = 0;
  }
}

void run_gemv_strict(const int16_t *x,
                     const int16_t *w,
                     int16_t *y,
                     int in_dim,
                     int out_dim,
                     uint32_t *profile) {
  if (profile) {
    clear_profile(profile);
    profile[kProfileInDim] = static_cast<uint32_t>(in_dim);
    profile[kProfileOutDim] = static_cast<uint32_t>(out_dim);
    profile[kProfileWeightElems] = static_cast<uint32_t>(in_dim * out_dim);
    profile[kProfileInputElems] = static_cast<uint32_t>(in_dim);
    profile[kProfileOutputElems] = static_cast<uint32_t>(out_dim);
    profile[kProfileMacs] = static_cast<uint32_t>(in_dim * out_dim);
    profile[kProfileTileOutIters] = static_cast<uint32_t>((out_dim + kTileOut - 1) / kTileOut);
  }

  for (int row = 0; row < out_dim; ++row) {
    int64_t acc = 0;
    const int row_base = row * in_dim;
    for (int i = 0; i < in_dim; ++i) {
      acc += static_cast<int32_t>(x[i]) * static_cast<int32_t>(w[row_base + i]);
    }
    y[row] = q8_8_round_shift(acc);
  }
}

void run_gemv_tiled_hls(const int16_t *x,
                        const int16_t *w,
                        int16_t *y,
                        int in_dim,
                        int out_dim,
                        uint32_t *profile) {
  if (profile) {
    clear_profile(profile);
    profile[kProfileInDim] = static_cast<uint32_t>(in_dim);
    profile[kProfileOutDim] = static_cast<uint32_t>(out_dim);
    profile[kProfileWeightElems] = static_cast<uint32_t>(in_dim * out_dim);
    profile[kProfileInputElems] = static_cast<uint32_t>(in_dim);
    profile[kProfileOutputElems] = static_cast<uint32_t>(out_dim);
    profile[kProfileMacs] = static_cast<uint32_t>(in_dim * out_dim);
    profile[kProfileTileOutIters] = static_cast<uint32_t>((out_dim + kTileOut - 1) / kTileOut);
  }

  int16_t x_local[kMaxInDim];
#pragma HLS ARRAY_PARTITION variable=x_local cyclic factor=8 dim=1

  load_x_tile(x, x_local, in_dim);

  for (int out_start = 0; out_start < out_dim; out_start += kTileOut) {
    const int valid_out = std::min(kTileOut, out_dim - out_start);
    compute_tile(x_local, w, y, in_dim, out_start, valid_out);
  }
}

}  // namespace linear
}  // namespace fpga
