CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wno-unknown-pragmas

ROOT_DIR := $(abspath ../../..)
BUILD_DIR := build
CSIM_BIN := $(BUILD_DIR)/test_fa_attention_kernel
XO := $(BUILD_DIR)/fa_attention_kernel.xo
PART ?= xck26-sfvc784-2LV-c
CLOCK_NS ?= 5.0

COMMON_SRCS := \
	../../common/fa_q8_8_attention.cpp

REF_SRCS := \
	$(ROOT_DIR)/cmodel/csrc/attention_math.cpp \
	$(ROOT_DIR)/cmodel/csrc/attention_kernels.cpp

CSIM_SRCS := \
	fa_attention_kernel.cpp \
	tb/test_fa_attention_kernel.cpp \
	$(COMMON_SRCS) \
	$(REF_SRCS)

CSIM_INCLUDES := \
	-I../../common \
	-I$(ROOT_DIR)/cmodel/csrc

.PHONY: csim xo clean

csim: $(CSIM_BIN)
	$(CSIM_BIN)

$(CSIM_BIN): $(CSIM_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CSIM_INCLUDES) $(CSIM_SRCS) -o $(CSIM_BIN)

xo:
	@mkdir -p $(BUILD_DIR)
	vitis_hls -f run_hls.tcl $(XO) $(PART) $(CLOCK_NS)

clean:
	rm -rf $(BUILD_DIR)
