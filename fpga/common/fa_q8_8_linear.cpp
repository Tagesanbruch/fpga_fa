#include "fa_q8_8_linear.hpp"
#include "fa_q8_8_dot.hpp"

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

    const int row = out_start + oi;
    const int row_base = row * in_dim;
    const int64_t acc = fpga::qmath::dotprod_q8_8_single(x_local, w + row_base, in_dim);
    y[row] = q8_8_round_shift(acc);
  }
}

void update_queue_profile(const GemvTaskDesc &task, uint32_t *profile) {
  if (!profile) return;
  profile[kProfileTaskCount] += 1;
  profile[kProfileTaskDescLoads] += 1;
  profile[kProfileQueueXElems] += task.in_dim;
  profile[kProfileQueueWElems] += static_cast<uint32_t>(task.in_dim * task.out_dim);
  profile[kProfileQueueYElems] += task.out_dim;
  profile[kProfileMacs] += static_cast<uint32_t>(task.in_dim * task.out_dim);
  profile[kProfileTileOutIters] += static_cast<uint32_t>((task.out_dim + kTileOut - 1) / kTileOut);
  profile[kProfileMaxInDim] = std::max<uint32_t>(profile[kProfileMaxInDim], task.in_dim);
  profile[kProfileMaxOutDim] = std::max<uint32_t>(profile[kProfileMaxOutDim], task.out_dim);
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

void run_gemv_queue_strict(const int16_t *x_all,
                           const int16_t *w_all,
                           int16_t *y_all,
                           const GemvTaskDesc *tasks,
                           int task_count,
                           uint32_t *profile) {
  if (profile) {
    clear_profile(profile);
    profile[kProfileQueueKernelCalls] = 1;
  }

  for (int task_idx = 0; task_idx < task_count; ++task_idx) {
    const GemvTaskDesc &task = tasks[task_idx];
    update_queue_profile(task, profile);
    run_gemv_strict(x_all + task.x_offset_elems, w_all + task.w_offset_elems, y_all + task.y_offset_elems,
                    task.in_dim, task.out_dim, nullptr);
  }
}

void run_gemv_queue_tiled_hls(const int16_t *x_all,
                              const int16_t *w_all,
                              int16_t *y_all,
                              const GemvTaskDesc *tasks,
                              int task_count,
                              uint32_t *profile) {
  if (profile) {
    clear_profile(profile);
    profile[kProfileQueueKernelCalls] = 1;
  }

  GemvTaskDesc task_local[kMaxQueueTasks];
#pragma HLS ARRAY_PARTITION variable=task_local complete dim=1

  const int bounded_tasks = std::min(task_count, kMaxQueueTasks);
  for (int task_idx = 0; task_idx < bounded_tasks; ++task_idx) {
#pragma HLS PIPELINE II=1
    task_local[task_idx] = tasks[task_idx];
  }

  for (int task_idx = 0; task_idx < bounded_tasks; ++task_idx) {
    const GemvTaskDesc &task = task_local[task_idx];
    update_queue_profile(task, profile);
    run_gemv_tiled_hls(x_all + task.x_offset_elems, w_all + task.w_offset_elems, y_all + task.y_offset_elems,
                       task.in_dim, task.out_dim, nullptr);
  }
}

}  // namespace linear
}  // namespace fpga
