#include "fa_q8_8_attention.hpp"
#include "fa_q8_8_fixed_point.hpp"
#include "fa_hls_stream_compat.hpp"
#include "fa_q8_8_o_normalize_block.hpp"
#include "fa_q8_8_online_softmax_ctx.hpp"
#include "fa_q8_8_qk_dotprod_slice.hpp"
#include "fa_q8_8_recip_nr_q16_16.hpp"
#include "fa_q8_8_row_context_rf.hpp"

#include <algorithm>
#include <cstddef>

namespace fpga {
namespace fa {

namespace {

struct ScorePacket {
  uint8_t qi;
  uint8_t kj;
  int16_t score;
};

static void generate_score_batch(const int16_t q_tile[kTileQ][kHeadDim],
                                 const int16_t k_tile[kTileK][kHeadDim],
                                 int q_start,
                                 int k_start,
                                 int batch_base,
                                 int valid_q,
                                 int valid_k,
                                 int16_t scale_q8_8,
                                 int16_t neg_large_q8_8,
                                 bool causal,
                                 hls::stream<ScorePacket> &score_stream);

static void consume_score_batch(const int16_t v_tile[kTileK][kHeadDim],
                                int16_t row_m[kTileQ],
                                uint32_t row_l[kTileQ],
                                int32_t row_acc[kTileQ][kHeadDim],
                                int batch_base,
                                int valid_q,
                                int valid_k,
                                hls::stream<ScorePacket> &score_stream);

static void process_score_batch(const int16_t q_tile[kTileQ][kHeadDim],
                                const int16_t k_tile[kTileK][kHeadDim],
                                const int16_t v_tile[kTileK][kHeadDim],
                                int16_t row_m[kTileQ],
                                uint32_t row_l[kTileQ],
                                int32_t row_acc[kTileQ][kHeadDim],
                                int q_start,
                                int k_start,
                                int batch_base,
                                int valid_q,
                                int valid_k,
                                int16_t scale_q8_8,
                                int16_t neg_large_q8_8,
                                bool causal) {
#pragma HLS INLINE off
  hls::stream<ScorePacket> score_stream("score_stream");
#pragma HLS STREAM variable=score_stream depth=64
#pragma HLS DATAFLOW
  generate_score_batch(q_tile, k_tile, q_start, k_start, batch_base, valid_q, valid_k, scale_q8_8, neg_large_q8_8,
                       causal, score_stream);
  consume_score_batch(v_tile, row_m, row_l, row_acc, batch_base, valid_q, valid_k, score_stream);
}

static void generate_score_batch(const int16_t q_tile[kTileQ][kHeadDim],
                                 const int16_t k_tile[kTileK][kHeadDim],
                                 int q_start,
                                 int k_start,
                                 int batch_base,
                                 int valid_q,
                                 int valid_k,
                                 int16_t scale_q8_8,
                                 int16_t neg_large_q8_8,
                                 bool causal,
                                 hls::stream<ScorePacket> &score_stream) {
#pragma HLS INLINE off
  const int remaining_rows = valid_q - batch_base;
  const int batch_rows = (remaining_rows > 0) ? std::min(kQPairBatchRows, remaining_rows) : 0;
  const int pair_count = (batch_rows + kRowPar - 1) / kRowPar;
  for (int kj = 0; kj < valid_k; ++kj) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=kTileK
    for (int slot = 0; slot < pair_count; ++slot) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=(kQPairBatchRows / kRowPar)
#pragma HLS PIPELINE II=1
      const int qi = batch_base + slot * kRowPar;
      const bool row0_valid = qi < valid_q;
      const bool row1_valid = (qi + 1) < valid_q;
      const int global_i0 = q_start + qi;
      const int global_i1 = q_start + qi + 1;
      const int global_j = k_start + kj;
      int64_t dp0 = 0;
      int64_t dp1 = 0;
      qk_dotprod_slice_pair(q_tile[qi], q_tile[qi + 1], k_tile[kj], dp0, dp1);

      int16_t score0 = neg_large_q8_8;
      int16_t score1 = neg_large_q8_8;
      if (row0_valid) {
        const int16_t dp_q8_8_0 = static_cast<int16_t>((dp0 >> 8) & 0xFFFF);
        score0 = q8_8_mul_sat(dp_q8_8_0, scale_q8_8);
        if (causal && global_j > global_i0) score0 = neg_large_q8_8;
      }
      if (row1_valid) {
        const int16_t dp_q8_8_1 = static_cast<int16_t>((dp1 >> 8) & 0xFFFF);
        score1 = q8_8_mul_sat(dp_q8_8_1, scale_q8_8);
        if (causal && global_j > global_i1) score1 = neg_large_q8_8;
      }

      if (row0_valid) {
        ScorePacket pkt0{};
        pkt0.qi = static_cast<uint8_t>(qi);
        pkt0.kj = static_cast<uint8_t>(kj);
        pkt0.score = score0;
        score_stream.write(pkt0);
      }
      if (row1_valid) {
        ScorePacket pkt1{};
        pkt1.qi = static_cast<uint8_t>(qi + 1);
        pkt1.kj = static_cast<uint8_t>(kj);
        pkt1.score = score1;
        score_stream.write(pkt1);
      }
    }
  }
}

static void consume_score_batch(const int16_t v_tile[kTileK][kHeadDim],
                                int16_t row_m[kTileQ],
                                uint32_t row_l[kTileQ],
                                int32_t row_acc[kTileQ][kHeadDim],
                                int batch_base,
                                int valid_q,
                                int valid_k,
                                hls::stream<ScorePacket> &score_stream) {
#pragma HLS INLINE off
  const int remaining_rows = valid_q - batch_base;
  const int batch_rows = (remaining_rows > 0) ? std::min(kQPairBatchRows, remaining_rows) : 0;
  const int total_packets = batch_rows * valid_k;
  for (int n = 0; n < total_packets; ++n) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=(kTileK * kQPairBatchRows)
#pragma HLS PIPELINE II=1
    const ScorePacket pkt = score_stream.read();
    const int qi = static_cast<int>(pkt.qi);
    const int kj = static_cast<int>(pkt.kj);
    online_softmax_ctx_acc_row_step(pkt.score, v_tile[kj], row_m[qi], row_l[qi], row_acc[qi]);
  }
}
}  // namespace

void clear_profile(uint32_t *profile) {
  if (!profile) return;
  for (int i = 0; i < kProfileWords; ++i) {
    profile[i] = 0;
  }
}

static void init_row_context(int16_t row_m[kTileQ],
                             uint32_t row_l[kTileQ],
                             int32_t row_acc[kTileQ][kHeadDim],
                             int valid_q,
                             int16_t neg_large_q8_8) {
  init_row_context_rf(row_m, row_l, row_acc, valid_q, neg_large_q8_8);
}

static void load_q_tile(const int16_t *q,
                        int16_t q_tile[kTileQ][kHeadDim],
                        int q_start,
                        int stride_elems,
                        int valid_q) {
  for (int qi = 0; qi < kTileQ; qi += kRowPar) {
    for (int d = 0; d < kHeadDim; ++d) {
#pragma HLS PIPELINE II=1
      const int16_t value0 = (qi < valid_q) ? q[(q_start + qi) * stride_elems + d] : 0;
      const int16_t value1 = ((qi + 1) < valid_q) ? q[(q_start + qi + 1) * stride_elems + d] : 0;
      q_tile[qi][d] = value0;
      if ((qi + 1) < kTileQ) q_tile[qi + 1][d] = value1;
    }
  }
}

static void load_kv_tile(const int16_t *k,
                         const int16_t *v,
                         int16_t k_tile[kTileK][kHeadDim],
                         int16_t v_tile[kTileK][kHeadDim],
                         int k_start,
                         int stride_elems,
                         int valid_k) {
  for (int kj = 0; kj < kTileK; ++kj) {
    for (int d = 0; d < kHeadDim; ++d) {
#pragma HLS PIPELINE II=1
      const int16_t k_value = (kj < valid_k) ? k[(k_start + kj) * stride_elems + d] : 0;
      const int16_t v_value = (kj < valid_k) ? v[(k_start + kj) * stride_elems + d] : 0;
      k_tile[kj][d] = k_value;
      v_tile[kj][d] = v_value;
    }
  }
}

static void compute_kv_tile(const int16_t q_tile[kTileQ][kHeadDim],
                            const int16_t k_tile[kTileK][kHeadDim],
                            const int16_t v_tile[kTileK][kHeadDim],
                            int16_t row_m[kTileQ],
                            uint32_t row_l[kTileQ],
                            int32_t row_acc[kTileQ][kHeadDim],
                            int q_start,
                            int k_start,
                            int valid_q,
                            int valid_k,
                            int16_t scale_q8_8,
                            int16_t neg_large_q8_8,
                            bool causal) {
#pragma HLS INLINE off
  for (int batch_idx = 0; batch_idx < kQPairBatchCount; ++batch_idx) {
    const int batch_base = batch_idx * kQPairBatchRows;
    if (batch_base >= valid_q) continue;
    process_score_batch(q_tile, k_tile, v_tile, row_m, row_l, row_acc, q_start, k_start, batch_base, valid_q, valid_k,
                        scale_q8_8, neg_large_q8_8, causal);
  }
}

static void normalize_tile(const int16_t row_m[kTileQ],
                           const uint32_t row_l[kTileQ],
                           const int32_t row_acc[kTileQ][kHeadDim],
                           int16_t o_tile[kTileQ][kHeadDim],
                           int valid_q) {
  (void)row_m;
  for (int qi = 0; qi < kTileQ; qi += kRowPar) {
    if (qi >= valid_q) continue;
    const bool row0_valid = qi < valid_q;
    const bool row1_valid = (qi + 1) < valid_q;
    if (row0_valid) o_normalize_block_row(row_acc[qi], row_l[qi], o_tile[qi]);
    if (row1_valid) o_normalize_block_row(row_acc[qi + 1], row_l[qi + 1], o_tile[qi + 1]);
  }
}

static void store_o_tile(const int16_t o_tile[kTileQ][kHeadDim],
                         int16_t *o,
                         int q_start,
                         int stride_elems,
                         int valid_q) {
  for (int qi = 0; qi < kTileQ; qi += kRowPar) {
    for (int d = 0; d < kHeadDim; ++d) {
#pragma HLS PIPELINE II=1
      if (qi < valid_q) {
        o[(q_start + qi) * stride_elems + d] = o_tile[qi][d];
      }
      if ((qi + 1) < valid_q) {
        o[(q_start + qi + 1) * stride_elems + d] = o_tile[qi + 1][d];
      }
    }
  }
}

void run_attention_tiled_hls(const int16_t *q,
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

  int16_t q_tile[kTileQ][kHeadDim];
  int16_t k_tile_ping[kTileK][kHeadDim];
  int16_t k_tile_pong[kTileK][kHeadDim];
  int16_t v_tile_ping[kTileK][kHeadDim];
  int16_t v_tile_pong[kTileK][kHeadDim];
  int16_t o_tile[kTileQ][kHeadDim];
  int16_t row_m[kTileQ];
  uint32_t row_l[kTileQ];
  int32_t row_acc[kTileQ][kHeadDim];

#pragma HLS ARRAY_PARTITION variable=row_m complete dim=1
#pragma HLS ARRAY_PARTITION variable=row_l complete dim=1
#pragma HLS ARRAY_PARTITION variable=q_tile cyclic factor=2 dim=1
#pragma HLS ARRAY_PARTITION variable=o_tile cyclic factor=2 dim=1
#pragma HLS ARRAY_PARTITION variable=row_acc cyclic factor=2 dim=1
#pragma HLS ARRAY_PARTITION variable=q_tile cyclic factor=8 dim=2
#pragma HLS ARRAY_PARTITION variable=k_tile_ping cyclic factor=8 dim=2
#pragma HLS ARRAY_PARTITION variable=k_tile_pong cyclic factor=8 dim=2
#pragma HLS ARRAY_PARTITION variable=v_tile_ping cyclic factor=8 dim=2
#pragma HLS ARRAY_PARTITION variable=v_tile_pong cyclic factor=8 dim=2
#pragma HLS ARRAY_PARTITION variable=o_tile cyclic factor=8 dim=2
#pragma HLS ARRAY_PARTITION variable=row_acc cyclic factor=8 dim=2

  for (int q_start = 0; q_start < seq_len; q_start += kTileQ) {
    const int valid_q = std::min(kTileQ, seq_len - q_start);

    load_q_tile(q, q_tile, q_start, stride_elems, valid_q);
    init_row_context(row_m, row_l, row_acc, valid_q, neg_large_q8_8);
    if (profile) {
      profile[kProfileInitRows] += static_cast<uint32_t>(valid_q);
      profile[kProfileLoadQElems] += static_cast<uint32_t>(valid_q * kHeadDim);
    }

    bool active_ping = true;
    int k_start = 0;
    int valid_k = std::min(kTileK, seq_len - k_start);
    load_kv_tile(k, v, k_tile_ping, v_tile_ping, k_start, stride_elems, valid_k);
    if (profile) {
      profile[kProfileLoadKVElems] += static_cast<uint32_t>(2 * valid_k * kHeadDim);
    }

    for (; k_start < seq_len; k_start += kTileK) {
      valid_k = std::min(kTileK, seq_len - k_start);
      if (profile) {
        profile[kProfileKVTileIters] += 1u;
        profile[kProfileValidScoreEvals] += static_cast<uint32_t>(valid_q * valid_k);
      }

      if (active_ping) {
        compute_kv_tile(q_tile, k_tile_ping, v_tile_ping, row_m, row_l, row_acc, q_start, k_start, valid_q, valid_k,
                        scale_q8_8, neg_large_q8_8, causal);
      } else {
        compute_kv_tile(q_tile, k_tile_pong, v_tile_pong, row_m, row_l, row_acc, q_start, k_start, valid_q, valid_k,
                        scale_q8_8, neg_large_q8_8, causal);
      }

      const int next_k_start = k_start + kTileK;
      if (next_k_start < seq_len) {
        const int next_valid_k = std::min(kTileK, seq_len - next_k_start);
        if (active_ping) {
          load_kv_tile(k, v, k_tile_pong, v_tile_pong, next_k_start, stride_elems, next_valid_k);
        } else {
          load_kv_tile(k, v, k_tile_ping, v_tile_ping, next_k_start, stride_elems, next_valid_k);
        }
        if (profile) {
          profile[kProfileLoadKVElems] += static_cast<uint32_t>(2 * next_valid_k * kHeadDim);
        }
        active_ping = !active_ping;
      }
    }

    normalize_tile(row_m, row_l, row_acc, o_tile, valid_q);
    store_o_tile(o_tile, o, q_start, stride_elems, valid_q);
    if (profile) {
      profile[kProfileNormalizeRows] += static_cast<uint32_t>(valid_q);
      profile[kProfileStoreOElems] += static_cast<uint32_t>(valid_q * kHeadDim);
    }
  }

  if (profile) {
    profile[kProfileScoreEvals] = static_cast<uint32_t>(seq_len * seq_len);
    profile[kProfileNormRows] = static_cast<uint32_t>(seq_len);
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
