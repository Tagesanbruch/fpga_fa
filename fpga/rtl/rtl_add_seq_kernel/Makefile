ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
BUILD_DIR ?= $(ROOT_DIR)/fpga/build/rtl_add_seq_kernel
XO ?= $(BUILD_DIR)/rtl_add_seq_kernel.xo
PART ?= xck26-sfvc784-2LV-c
VIVADO ?= vivado

.PHONY: xo clean

xo:
	mkdir -p $(BUILD_DIR)
	$(VIVADO) -mode batch -source package_kernel.tcl -tclargs $(XO) $(PART)

clean:
	rm -rf $(BUILD_DIR)
