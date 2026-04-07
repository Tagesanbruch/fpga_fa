proc create_root_design { parentCell } {
  if { $parentCell eq "" } {
    set parentCell [get_bd_cells /]
  }

  current_bd_instance $parentCell

  set PS_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e PS_0 ]
  apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1"} [get_bd_cells PS_0]

  set_property -dict [list \
    CONFIG.PSU__USE__M_AXI_GP0 {1} \
    CONFIG.PSU__USE__S_AXI_HPC0_FPD {1} \
    CONFIG.PSU__USE__S_AXI_HPC1_FPD {1} \
    CONFIG.PSU__USE__S_AXI_HP3_FPD {1} \
    CONFIG.PSU__FPGA_PL0_ENABLE {1} \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {100} \
  ] $PS_0

  set clk_wiz_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:clk_wiz clk_wiz_0 ]
  set_property -dict [list \
    CONFIG.PRIM_SOURCE {No_buffer} \
    CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {300.000} \
    CONFIG.CLKOUT2_USED {true} \
    CONFIG.CLKOUT2_REQUESTED_OUT_FREQ {100.000} \
    CONFIG.CLK_OUT1_PORT {clk_300M} \
    CONFIG.CLK_OUT2_PORT {clk_100M} \
    CONFIG.RESET_TYPE {ACTIVE_LOW} \
  ] $clk_wiz_0

  set proc_sys_reset_300MHz [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_300MHz ]
  set proc_sys_reset_100MHz [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset proc_sys_reset_100MHz ]

  connect_bd_net [get_bd_pins PS_0/pl_clk0] [get_bd_pins clk_wiz_0/clk_in1]
  connect_bd_net [get_bd_pins PS_0/pl_resetn0] [get_bd_pins clk_wiz_0/resetn]

  connect_bd_net [get_bd_pins clk_wiz_0/clk_300M] [get_bd_pins PS_0/maxihpm0_fpd_aclk]
  connect_bd_net [get_bd_pins clk_wiz_0/clk_300M] [get_bd_pins PS_0/saxihpc0_fpd_aclk]
  connect_bd_net [get_bd_pins clk_wiz_0/clk_300M] [get_bd_pins PS_0/saxihpc1_fpd_aclk]
  connect_bd_net [get_bd_pins clk_wiz_0/clk_300M] [get_bd_pins PS_0/saxihp3_fpd_aclk]
  connect_bd_net [get_bd_pins clk_wiz_0/clk_300M] [get_bd_pins proc_sys_reset_300MHz/slowest_sync_clk]
  connect_bd_net [get_bd_pins PS_0/pl_resetn0] [get_bd_pins proc_sys_reset_300MHz/ext_reset_in]

  connect_bd_net [get_bd_pins clk_wiz_0/clk_100M] [get_bd_pins PS_0/maxihpm0_lpd_aclk]
  connect_bd_net [get_bd_pins clk_wiz_0/clk_100M] [get_bd_pins proc_sys_reset_100MHz/slowest_sync_clk]
  connect_bd_net [get_bd_pins PS_0/pl_resetn0] [get_bd_pins proc_sys_reset_100MHz/ext_reset_in]

  set_property PFM_NAME {openai.local:kv260:kv260_min_shell:1.0} [get_files [current_bd_design].bd]
  set_property PFM.AXI_PORT {M_AXI_HPM0_FPD {memport "M_AXI_GP" sptag "" memory "" is_range "false"} S_AXI_HPC0_FPD {memport "S_AXI_HP" sptag "HPC0" memory "PS_0 HPC0_DDR_LOW" is_range "false"} S_AXI_HPC1_FPD {memport "S_AXI_HP" sptag "HPC1" memory "PS_0 HPC1_DDR_LOW" is_range "false"} S_AXI_HP3_FPD {memport "S_AXI_HP" sptag "HP3" memory "PS_0 HP3_DDR_LOW" is_range "false"}} [get_bd_cells /PS_0]
  set_property PFM.CLOCK {clk_300M {id "0" is_default "true" proc_sys_reset "/proc_sys_reset_300MHz" status "fixed"} clk_100M {id "1" is_default "false" proc_sys_reset "/proc_sys_reset_100MHz" status "fixed"}} [get_bd_cells /clk_wiz_0]

  validate_bd_design
  save_bd_design
}

create_root_design ""
