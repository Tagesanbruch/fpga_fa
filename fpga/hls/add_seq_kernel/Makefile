CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wno-unknown-pragmas

ROOT_DIR := $(abspath ../../..)
BUILD_DIR := build
CSIM_BIN := $(BUILD_DIR)/test_add_seq_kernel
XO := $(BUILD_DIR)/add_seq_kernel.xo
PART ?= xck26-sfvc784-2LV-c
CLOCK_NS ?= 5.0
SOURCE_XILINX_ENV := $(ROOT_DIR)/fpga/remote/source_xilinx_env.sh

CSIM_SRCS := \
	add_seq_kernel.cpp \
	tb/test_add_seq_kernel.cpp

.PHONY: csim xo clean

csim: $(CSIM_BIN)
	$(CSIM_BIN)

$(CSIM_BIN): $(CSIM_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CSIM_SRCS) -o $(CSIM_BIN)

xo:
	@mkdir -p $(BUILD_DIR)
	bash -lc 'source "$(SOURCE_XILINX_ENV)" hls && vitis_hls run_hls.tcl "$(XO)" "$(PART)" "$(CLOCK_NS)"'

clean:
	rm -rf $(BUILD_DIR)
