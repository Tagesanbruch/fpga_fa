CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wno-unknown-pragmas

ROOT_DIR := $(abspath ../../..)
BUILD_DIR := build
CSIM_BIN := $(BUILD_DIR)/test_fa_attention_kernel
XO := $(BUILD_DIR)/fa_attention_kernel.xo
PART ?= xck26-sfvc784-2LV-c
CLOCK_NS ?= 5.0
SOURCE_XILINX_ENV := $(ROOT_DIR)/fpga/remote/source_xilinx_env.sh

COMMON_SRCS := \
	../../common/fa_q8_8_attention.cpp \
	../../common/fa_q8_8_fixed_point.cpp \
	../../common/fa_q8_8_recip_nr_q16_16.cpp \
	../../common/fa_q8_8_qk_dotprod_slice.cpp \
	../../common/fa_q8_8_online_softmax_ctx.cpp \
	../../common/fa_q8_8_o_normalize_block.cpp \
	../../common/fa_q8_8_row_context_rf.cpp

REF_SRCS := \
	$(ROOT_DIR)/cmodel/csrc/attention_math.cpp \
	$(ROOT_DIR)/cmodel/csrc/attention_kernels.cpp \
	$(ROOT_DIR)/cmodel/csrc/attention_experiment.cpp

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
	bash -lc 'source "$(SOURCE_XILINX_ENV)" hls && vitis_hls run_hls.tcl "$(XO)" "$(PART)" "$(CLOCK_NS)"'

clean:
	rm -rf $(BUILD_DIR)
