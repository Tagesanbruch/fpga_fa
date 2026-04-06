#include "fa_q8_8_recip_nr_q16_16.hpp"

#include "fa_q8_8_fixed_point.hpp"

namespace fpga {
namespace fa {

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

}  // namespace fa
}  // namespace fpga
