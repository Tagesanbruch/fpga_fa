CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra

XRT_ROOT ?= $(XILINX_XRT)
ifneq ($(strip $(XRT_ROOT)),)
XRT_INC ?= $(XRT_ROOT)/include
XRT_LIB ?= $(XRT_ROOT)/lib
else ifneq ($(wildcard /usr/include/xrt/xrt_bo.h),)
XRT_INC ?= /usr/include
ifneq ($(wildcard /usr/lib/x86_64-linux-gnu/libxrt_coreutil.so),)
XRT_LIB ?= /usr/lib/x86_64-linux-gnu
else ifneq ($(wildcard /usr/lib/aarch64-linux-gnu/libxrt_coreutil.so),)
XRT_LIB ?= /usr/lib/aarch64-linux-gnu
else ifneq ($(wildcard /lib/libxrt_coreutil.so),)
XRT_LIB ?= /lib
endif
else ifneq ($(wildcard /opt/xilinx/xrt/include/xrt/xrt_bo.h),)
XRT_INC ?= /opt/xilinx/xrt/include
XRT_LIB ?= /opt/xilinx/xrt/lib
endif

BIN := add_seq_runner
SRCS := add_seq_runner.cpp
LDFLAGS += -L$(XRT_LIB) -lxrt_coreutil -luuid

.PHONY: all clean check-xrt

all: $(BIN)

check-xrt:
	@test -n "$(XRT_INC)" || (echo "[ERR] XRT headers not found. Set XILINX_XRT or install the XRT development package." && exit 1)
	@test -n "$(XRT_LIB)" || (echo "[ERR] XRT libraries not found. Set XILINX_XRT or install the XRT development package." && exit 1)

$(BIN): check-xrt $(SRCS)
	$(CXX) $(CXXFLAGS) -I$(XRT_INC) $(SRCS) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)
