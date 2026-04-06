#include "fa_q8_8_attention.hpp"

#include <algorithm>
#include <cstddef>

namespace fpga {
namespace fa {

namespace {

int16_t sat_s16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

uint32_t to_u32(uint64_t v) { return static_cast<uint32_t>(v & 0xFFFFFFFFu); }

int16_t q8_8_mul_sat(int16_t a, int16_t b) {
  const int32_t prod = static_cast<int32_t>(a) * static_cast<int32_t>(b);
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
  const int y = y0 - (((y0 - y1) * frac) >> 8);
  return static_cast<uint16_t>(y & 0xFFFF);
}

uint32_t mul_q1_31(uint32_t a, uint32_t b) {
  const uint64_t pp_hh = static_cast<uint64_t>(a >> 16) * static_cast<uint64_t>(b >> 16);
  const uint64_t pp_hl = static_cast<uint64_t>(a >> 16) * static_cast<uint64_t>(b & 0xFFFFu);
  const uint64_t pp_lh = static_cast<uint64_t>(a & 0xFFFFu) * static_cast<uint64_t>(b >> 16);
  const uint64_t pp_ll = static_cast<uint64_t>(a & 0xFFFFu) * static_cast<uint64_t>(b & 0xFFFFu);
  const uint64_t acc = (pp_hh << 32) + (pp_hl << 16) + (pp_lh << 16) + pp_ll;
  return static_cast<uint32_t>(acc >> 32);
}

uint32_t mul_q1_31_corr(uint32_t a, uint32_t b, bool corr_ov) {
  const uint64_t pp_hh = static_cast<uint64_t>(a >> 16) * static_cast<uint64_t>(b >> 16);
  const uint64_t pp_hl = static_cast<uint64_t>(a >> 16) * static_cast<uint64_t>(b & 0xFFFFu);
  const uint64_t pp_lh = static_cast<uint64_t>(a & 0xFFFFu) * static_cast<uint64_t>(b >> 16);
  const uint64_t pp_ll = static_cast<uint64_t>(a & 0xFFFFu) * static_cast<uint64_t>(b & 0xFFFFu);
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

uint32_t recip_nr_rtl_q16_16(uint32_t x_q16_16) {
  static const uint32_t lut[32] = {
      0xFC0FC0FCu, 0xF4898D60u, 0xED7303B6u, 0xE6C2B448u,
      0xE070381Cu, 0xDA740DA7u, 0xD4C77B03u, 0xCF6474A9u,
      0xCA4587E7u, 0xC565C87Bu, 0xC0C0C0C1u, 0xBC52640Cu,
      0xB81702E0u, 0xB40B40B4u, 0xB02C0B03u, 0xAC769184u,
      0xA8E83F57u, 0xA57EB503u, 0xA237C32Bu, 0x9F1165E7u,
      0x9C09C09Cu, 0x991F1A51u, 0x964FDA6Cu, 0x939A85C4u,
      0x90FDBC09u, 0x8E78356Du, 0x8C08C08Cu, 0x89AE408Au,
      0x8767AB5Fu, 0x85340853u, 0x83126E98u, 0x81020408u,
  };

  const int lz = clz32_cpp(x_q16_16);
  const uint32_t d_norm = x_q16_16 << lz;
  const bool is_zero = (x_q16_16 == 0);
  const bool is_one = (x_q16_16 == 1);

  const uint32_t r0 = lut[(d_norm >> 26) & 0x1Fu];
  const uint32_t dr0_q1_31 = mul_q1_31(d_norm, r0);
  const uint64_t corr1_w = (1ull << 32) - static_cast<uint64_t>(dr0_q1_31);
  const uint32_t corr1 = static_cast<uint32_t>(corr1_w & 0xFFFFFFFFu);
  const bool corr1_ov = ((corr1_w >> 32) & 0x1u) != 0;
  const uint32_t r1 = mul_q1_31_corr(r0, corr1, corr1_ov);

  const uint32_t dr1_q1_31 = mul_q1_31(d_norm, r1);
  const uint64_t corr2_w = (1ull << 32) - static_cast<uint64_t>(dr1_q1_31);
  const uint32_t corr2 = static_cast<uint32_t>(corr2_w & 0xFFFFFFFFu);
  const bool corr2_ov = ((corr2_w >> 32) & 0x1u) != 0;
  const uint32_t r2 = mul_q1_31_corr(r1, corr2, corr2_ov);

  if (is_zero || is_one) return 0xFFFFFFFFu;

  if (lz >= 31) {
    const uint64_t result_wide = static_cast<uint64_t>(r2) << (lz - 31);
    return (result_wide >> 32) ? 0xFFFFFFFFu : static_cast<uint32_t>(result_wide);
  }
  return r2 >> (31 - lz);
}

}  // namespace

void clear_profile(uint32_t *profile) {
  if (!profile) return;
  for (int i = 0; i < kProfileWords; ++i) {
    profile[i] = 0;
  }
}

void run_attention_strict(const int16_t *q,
                          const int16_t *k,
                          const int16_t *v,
                          int16_t *o,
                          int seq_len,
                          int stride_bytes,
                          int16_t scale_q8_8,
                          int16_t neg_large_q8_8,
                          bool causal,
                          uint32_t *profile) {
  if (!q || !k || !v || !o) return;

  if (seq_len < 0) seq_len = 0;
  if (seq_len > kMaxSeqLen) seq_len = kMaxSeqLen;

  int stride_elems = stride_bytes / static_cast<int>(sizeof(int16_t));
  if (stride_elems < kHeadDim) stride_elems = kHeadDim;

  clear_profile(profile);
  if (profile) {
    profile[kProfileSeqLen] = static_cast<uint32_t>(seq_len);
    profile[kProfileQTiles] = static_cast<uint32_t>((seq_len + kTileQ - 1) / kTileQ);
    profile[kProfileKTiles] = static_cast<uint32_t>((seq_len + kTileK - 1) / kTileK);
    profile[kProfileStrideBytes] = static_cast<uint32_t>(stride_bytes);
    profile[kProfileScaleQ8_8] = static_cast<uint16_t>(scale_q8_8);
    profile[kProfileCausal] = causal ? 1u : 0u;
  }

  for (int i = 0; i < seq_len; ++i) {
    for (int d = 0; d < kHeadDim; ++d) {
      o[i * stride_elems + d] = 0;
    }
  }

  for (int q_start = 0; q_start < seq_len; q_start += kTileQ) {
    const int valid_q = std::min(kTileQ, seq_len - q_start);

    int16_t row_m[kTileQ];
    uint32_t row_l[kTileQ];
    int64_t row_acc[kTileQ][kHeadDim];

    for (int qi = 0; qi < valid_q; ++qi) {
      row_m[qi] = neg_large_q8_8;
      row_l[qi] = 0;
      for (int d = 0; d < kHeadDim; ++d) {
        row_acc[qi][d] = 0;
      }
    }

    for (int k_start = 0; k_start < seq_len; k_start += kTileK) {
      const int valid_k = std::min(kTileK, seq_len - k_start);

      for (int qi = 0; qi < valid_q; ++qi) {
        const int global_i = q_start + qi;
        const int q_row = global_i * stride_elems;

        for (int kj = 0; kj < valid_k; ++kj) {
          const int global_j = k_start + kj;
          const int k_row = global_j * stride_elems;
          const int v_row = global_j * stride_elems;

          int64_t dp = 0;
          for (int d = 0; d < kHeadDim; ++d) {
            dp += static_cast<int32_t>(q[q_row + d]) * static_cast<int32_t>(k[k_row + d]);
          }

          int16_t dp_q8_8 = static_cast<int16_t>((dp >> 8) & 0xFFFF);
          int16_t score = q8_8_mul_sat(dp_q8_8, scale_q8_8);
          if (causal && global_j > global_i) {
            score = neg_large_q8_8;
          }

          const int16_t m_old = row_m[qi];
          const int16_t m_new = (score > m_old) ? score : m_old;
          const int16_t diff_old = static_cast<int16_t>(m_old - m_new);
          const int16_t diff_new = static_cast<int16_t>(score - m_new);
          const uint16_t exp_old = exp_pwl_q1_15(diff_old);
          const uint16_t exp_new = exp_pwl_q1_15(diff_new);

          const uint32_t l_scaled = static_cast<uint32_t>((static_cast<uint64_t>(row_l[qi]) * exp_old) >> 15);
          const uint32_t l_term = static_cast<uint32_t>(exp_new) << 1;
          row_l[qi] = to_u32(static_cast<uint64_t>(l_scaled) + l_term);

          for (int d = 0; d < kHeadDim; ++d) {
            const int64_t acc_old_sc = (row_acc[qi][d] * static_cast<int64_t>(exp_old)) >> 15;
            const int64_t pv_term =
                (static_cast<int64_t>(exp_new) * static_cast<int64_t>(static_cast<int32_t>(v[v_row + d]))) << 1;
            row_acc[qi][d] = acc_old_sc + pv_term;
          }

          row_m[qi] = m_new;
        }
      }
    }

    for (int qi = 0; qi < valid_q; ++qi) {
      const uint32_t recip = recip_nr_rtl_q16_16(row_l[qi]);
      for (int d = 0; d < kHeadDim; ++d) {
        const __int128 norm_mul_q32_32 =
            static_cast<__int128>(row_acc[qi][d]) * static_cast<int64_t>(static_cast<int32_t>(recip));
        const __int128 norm_rounded_q32_32 =
            (norm_mul_q32_32 >= 0) ? (norm_mul_q32_32 + static_cast<__int128>(2147483648ll))
                                   : (norm_mul_q32_32 - static_cast<__int128>(2147483648ll));
        const int64_t norm_result = static_cast<int64_t>(norm_rounded_q32_32 >> 32);
        o[(q_start + qi) * stride_elems + d] = sat_s16(static_cast<int32_t>(norm_result));
      }
    }
  }

  if (profile) {
    profile[kProfileScoreEvals] = static_cast<uint32_t>(seq_len * seq_len);
    profile[kProfileNormRows] = static_cast<uint32_t>(seq_len);
  }
}

}  // namespace fa
}  // namespace fpga
