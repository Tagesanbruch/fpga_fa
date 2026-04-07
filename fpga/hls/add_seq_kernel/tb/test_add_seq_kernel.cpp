#include <cstdint>
#include <iostream>
#include <vector>

extern "C" void add_seq_kernel(const uint32_t* in, uint32_t* out, int length, uint32_t base_add);

int main() {
  constexpr int kLength = 256;
  constexpr uint32_t kBaseAdd = 1;

  std::vector<uint32_t> input(kLength);
  std::vector<uint32_t> output(kLength, 0);

  for (int i = 0; i < kLength; ++i) {
    input[i] = static_cast<uint32_t>(1000 + i * 7);
  }

  add_seq_kernel(input.data(), output.data(), kLength, kBaseAdd);

  for (int i = 0; i < kLength; ++i) {
    const uint32_t expect = input[i] + kBaseAdd + static_cast<uint32_t>(i);
    if (output[i] != expect) {
      std::cerr << "[FAIL] mismatch at " << i << ": got=" << output[i] << " expect=" << expect << "\n";
      return 1;
    }
  }

  std::cout << "[PASS] add_seq_kernel csim passed for " << kLength << " words\n";
  return 0;
}
