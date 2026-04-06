#pragma once

#include <cstdint>

namespace fpga {
namespace fa {

int16_t sat_s16(int32_t v);
int32_t to_s32(int64_t v);
uint32_t to_u32(uint64_t v);

int16_t q8_8_mul_sat(int16_t a, int16_t b);
uint16_t exp_pwl_q1_15(int16_t x_q8_8);
uint16_t exp2_ctx_q1_15(int16_t x_q8_8);

uint32_t mul_q1_31(uint32_t a, uint32_t b);
uint32_t mul_q1_31_corr(uint32_t a, uint32_t b, bool corr_ov);
int clz32_cpp(uint32_t val);

}  // namespace fa
}  // namespace fpga
