CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra

XRT_ROOT ?= $(XILINX_XRT)
XRT_INC ?= $(XRT_ROOT)/include
XRT_LIB ?= $(XRT_ROOT)/lib

ROOT_DIR := $(abspath ../../..)
COMMON_SRCS := \
	../../common/fa_q8_8_attention.cpp

BIN := fa_xrt_runner
SRCS := fa_xrt_runner.cpp $(COMMON_SRCS)

INCLUDES := -I../../common
LDFLAGS += -L$(XRT_LIB) -lxrt_coreutil -luuid

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) -I$(XRT_INC) $(INCLUDES) $(SRCS) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)
