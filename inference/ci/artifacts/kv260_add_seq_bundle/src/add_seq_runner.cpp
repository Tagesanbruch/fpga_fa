#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if __has_include(<xrt/xrt_bo.h>)
#include <xrt/xrt_bo.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_uuid.h>
#include <xrt/xrt_xclbin.h>
#elif __has_include(<experimental/xrt_bo.h>)
#include <experimental/xrt_bo.h>
#include <experimental/xrt_device.h>
#include <experimental/xrt_kernel.h>
#include <experimental/xrt_xclbin.h>
#else
#error "XRT native C++ headers were not found. Set XILINX_XRT before building."
#endif

namespace {

struct Options {
  std::string xclbin_path;
  std::string kernel_name = "add_seq_kernel";
  int device_index = 0;
  int length = 1024;
  uint32_t base_add = 1;
  bool dump = false;
};

void usage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " --xclbin <file> [--kernel <name>] [--device <idx>] [--length <n>] [--base-add <n>] [--dump]\n";
}

Options parse_args(int argc, char** argv) {
  Options opt;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--xclbin" && i + 1 < argc) {
      opt.xclbin_path = argv[++i];
    } else if (arg == "--kernel" && i + 1 < argc) {
      opt.kernel_name = argv[++i];
    } else if (arg == "--device" && i + 1 < argc) {
      opt.device_index = std::stoi(argv[++i]);
    } else if (arg == "--length" && i + 1 < argc) {
      opt.length = std::stoi(argv[++i]);
    } else if (arg == "--base-add" && i + 1 < argc) {
      opt.base_add = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--dump") {
      opt.dump = true;
    } else {
      usage(argv[0]);
      throw std::runtime_error("invalid arguments");
    }
  }
  if (opt.xclbin_path.empty()) {
    usage(argv[0]);
    throw std::runtime_error("missing --xclbin");
  }
  if (opt.length <= 0) {
    throw std::runtime_error("length must be positive");
  }
  return opt;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options opt = parse_args(argc, argv);

    std::vector<uint32_t> input(static_cast<size_t>(opt.length));
    std::vector<uint32_t> output(static_cast<size_t>(opt.length), 0);
    for (int i = 0; i < opt.length; ++i) {
      input[static_cast<size_t>(i)] = static_cast<uint32_t>(1000 + i * 7);
    }

    xrt::device device(opt.device_index);
    xrt::xclbin xclbin(opt.xclbin_path);
    auto uuid = device.load_xclbin(xclbin);
    xrt::kernel kernel(device, uuid, opt.kernel_name);

    const size_t bytes = input.size() * sizeof(uint32_t);
    xrt::bo in_bo(device, bytes, kernel.group_id(0));
    xrt::bo out_bo(device, bytes, kernel.group_id(1));

    std::memcpy(in_bo.map<void*>(), input.data(), bytes);
    std::memset(out_bo.map<void*>(), 0, bytes);

    in_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    auto run = kernel(in_bo, out_bo, opt.length, opt.base_add);
    run.wait();
    out_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    std::memcpy(output.data(), out_bo.map<void*>(), bytes);

    for (int i = 0; i < opt.length; ++i) {
      const uint32_t expect = input[static_cast<size_t>(i)] + opt.base_add + static_cast<uint32_t>(i);
      if (output[static_cast<size_t>(i)] != expect) {
        std::cerr << "[FAIL] mismatch at " << i << ": got=" << output[static_cast<size_t>(i)]
                  << " expect=" << expect << "\n";
        return 1;
      }
    }

    std::cout << "[PASS] add_seq kernel verified, length=" << opt.length << " base_add=" << opt.base_add << "\n";
    if (opt.dump) {
      const int show = std::min(opt.length, 16);
      for (int i = 0; i < show; ++i) {
        std::cout << "  [" << i << "] " << input[static_cast<size_t>(i)] << " -> "
                  << output[static_cast<size_t>(i)] << "\n";
      }
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "[ERR] " << e.what() << "\n";
    return 1;
  }
}
