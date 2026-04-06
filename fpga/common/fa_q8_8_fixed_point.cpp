#include "fa_q8_8_fixed_point.hpp"

namespace fpga {
namespace fa {

int16_t sat_s16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

int32_t to_s32(int64_t v) {
  const uint32_t u = static_cast<uint32_t>(v & 0xFFFFFFFFu);
  return static_cast<int32_t>(u);
}

uint32_t to_u32(uint64_t v) { return static_cast<uint32_t>(v & 0xFFFFFFFFu); }

int16_t q8_8_mul_sat(int16_t a, int16_t b) {
  const int32_t prod = static_cast<int32_t>(a) * static_cast<int32_t>(b);
#pragma HLS bind_op variable=prod op=mul impl=dsp
  const int32_t rounded = (prod >= 0) ? (prod + 128) : (prod - 128);
  return sat_s16(rounded >> 8);
}

uint16_t exp_pwl_q1_15(int16_t x_q8_8) {
  int32_t x = x_q8_8;
  if (x > 0) x = 0;
  if (x < -2048) x = -2048;

  const int32_t u_q8_8 = -x;
  const int u_int = (u_q8_8 >> 8) & 0xFF;
  const int seg_idx = (u_int >= 8) ? 7 : ((u_q8_8 >> 8) & 0x7);
  const int frac = (u_int >= 8) ? 255 : (u_q8_8 & 0xFF);

  static const int table[8][2] = {
      {32767, 12055}, {12055, 4431}, {4431, 1631}, {1631, 600},
      {600, 221},     {221, 81},     {81, 30},     {30, 11},
  };

  const int y0 = table[seg_idx][0];
  const int y1 = table[seg_idx][1];
  const int interp = (y0 - y1) * frac;
#pragma HLS bind_op variable=interp op=mul impl=dsp
  const int y = y0 - (interp >> 8);
  return static_cast<uint16_t>(y & 0xFFFF);
}

uint16_t exp2_ctx_q1_15(int16_t x_q8_8) {
  int32_t x = x_q8_8;
  if (x > 0) x = 0;
  if (x < -4096) x = -4096;

  static const uint16_t table[32] = {
      32768, 32066, 31379, 30706, 30048, 29405, 28774, 28158,
      27554, 26964, 26386, 25821, 25268, 24726, 24196, 23678,
      23170, 22674, 22188, 21713, 21247, 20792, 20347, 19911,
      19484, 19066, 18658, 18258, 17867, 17484, 17109, 16743,
  };

  const int32_t z_q8_8 = (((-x) * 369) + 128) >> 8;
  const int int_part = (z_q8_8 >> 8) & 0xFF;
  const int frac_idx = (z_q8_8 >> 3) & 0x1F;
  if (int_part >= 16) return 0;
  return static_cast<uint16_t>(table[frac_idx] >> int_part);
}

uint32_t mul_q1_31(uint32_t a, uint32_t b) {
  const uint64_t pp_hh = static_cast<uint64_t>(a >> 16) * static_cast<uint64_t>(b >> 16);
  const uint64_t pp_hl = static_cast<uint64_t>(a >> 16) * static_cast<uint64_t>(b & 0xFFFFu);
  const uint64_t pp_lh = static_cast<uint64_t>(a & 0xFFFFu) * static_cast<uint64_t>(b >> 16);
  const uint64_t pp_ll = static_cast<uint64_t>(a & 0xFFFFu) * static_cast<uint64_t>(b & 0xFFFFu);
#pragma HLS bind_op variable=pp_hh op=mul impl=dsp
#pragma HLS bind_op variable=pp_hl op=mul impl=dsp
#pragma HLS bind_op variable=pp_lh op=mul impl=dsp
#pragma HLS bind_op variable=pp_ll op=mul impl=dsp
  const uint64_t acc = (pp_hh << 32) + (pp_hl << 16) + (pp_lh << 16) + pp_ll;
  return static_cast<uint32_t>(acc >> 32);
}

uint32_t mul_q1_31_corr(uint32_t a, uint32_t b, bool corr_ov) {
  const uint64_t pp_hh = static_cast<uint64_t>(a >> 16) * static_cast<uint64_t>(b >> 16);
  const uint64_t pp_hl = static_cast<uint64_t>(a >> 16) * static_cast<uint64_t>(b & 0xFFFFu);
  const uint64_t pp_lh = static_cast<uint64_t>(a & 0xFFFFu) * static_cast<uint64_t>(b >> 16);
  const uint64_t pp_ll = static_cast<uint64_t>(a & 0xFFFFu) * static_cast<uint64_t>(b & 0xFFFFu);
#pragma HLS bind_op variable=pp_hh op=mul impl=dsp
#pragma HLS bind_op variable=pp_hl op=mul impl=dsp
#pragma HLS bind_op variable=pp_lh op=mul impl=dsp
#pragma HLS bind_op variable=pp_ll op=mul impl=dsp
  const uint64_t acc = (pp_hh << 32) + (pp_hl << 16) + (pp_lh << 16) + pp_ll;
  return corr_ov ? a : static_cast<uint32_t>(acc >> 31);
}

int clz32_cpp(uint32_t val) {
  if (val == 0) return 32;
  int n = 0;
  uint32_t x = val;
  if ((x >> 16) == 0) {
    n += 16;
    x <<= 16;
  }
  if ((x >> 24) == 0) {
    n += 8;
    x <<= 8;
  }
  if ((x >> 28) == 0) {
    n += 4;
    x <<= 4;
  }
  if ((x >> 30) == 0) {
    n += 2;
    x <<= 2;
  }
  if ((x >> 31) == 0) {
    n += 1;
  }
  return n;
}

}  // namespace fa
}  // namespace fpga
