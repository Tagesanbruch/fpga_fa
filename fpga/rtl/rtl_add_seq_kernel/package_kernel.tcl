set script_dir [file normalize [file dirname [info script]]]
set xo_path    [file normalize [expr {[llength $argv] >= 1 ? [lindex $argv 0] : [file join $script_dir build rtl_add_seq_kernel.xo]}]]
set part       [expr {[llength $argv] >= 2 ? [lindex $argv 1] : "xck26-sfvc784-2LV-c"}]

set build_dir [file dirname $xo_path]
set proj_dir  [file join $build_dir vivado_proj]
set ip_dir    [file join $build_dir ip]
set tmp_dir   [file join $build_dir tmp_edit]

if {[file exists $xo_path]} {
  file delete -force $xo_path
}

file mkdir $build_dir
file mkdir $proj_dir
file mkdir $ip_dir
file mkdir $tmp_dir

create_project -force rtl_add_seq_kernel_pkg $proj_dir -part $part
add_files -norecurse [glob -directory [file join $script_dir src] *.v]
set_property top rtl_add_seq_kernel [current_fileset]
update_compile_order -fileset sources_1

ipx::package_project -root_dir $ip_dir -vendor openai.com -library RTLKernel -taxonomy /KernelIP -import_files -set_current true
set core [ipx::current_core]
set_property name rtl_add_seq_kernel $core
set_property display_name {rtl_add_seq_kernel} $core
set_property description {Minimal RTL add-seq kernel for KV260 XRT validation} $core
set_property sdx_kernel true $core
set_property sdx_kernel_type rtl $core

set clk_if [ipx::get_bus_interfaces ap_clk -of_objects $core]
set rst_if [ipx::get_bus_interfaces ap_rst_n -of_objects $core]
set s_axi_if [ipx::get_bus_interfaces s_axi_control -of_objects $core]
set m_axi0_if [ipx::get_bus_interfaces m_axi_gmem0 -of_objects $core]
set m_axi1_if [ipx::get_bus_interfaces m_axi_gmem1 -of_objects $core]

if {$s_axi_if ne ""} {
  ipx::associate_bus_interfaces -busif s_axi_control -clock ap_clk $core
}
if {$m_axi0_if ne ""} {
  ipx::associate_bus_interfaces -busif m_axi_gmem0 -clock ap_clk $core
}
if {$m_axi1_if ne ""} {
  ipx::associate_bus_interfaces -busif m_axi_gmem1 -clock ap_clk $core
}

if {$clk_if ne ""} {
  set_property value {s_axi_control:m_axi_gmem0:m_axi_gmem1} [ipx::get_bus_parameters ASSOCIATED_BUSIF -of_objects $clk_if]
  set_property value {ap_rst_n} [ipx::get_bus_parameters ASSOCIATED_RESET -of_objects $clk_if]
}
if {$rst_if ne ""} {
  set_property value ACTIVE_LOW [ipx::get_bus_parameters POLARITY -of_objects $rst_if]
}

ipx::create_xgui_files $core
ipx::update_checksums $core
ipx::save_core $core
close_project

package_xo -xo_path $xo_path -kernel_name rtl_add_seq_kernel -ip_directory $ip_dir -kernel_xml [file join $script_dir kernel.xml] -ctrl_protocol ap_ctrl_chain
puts [format {INFO: RTL XO generated at %s} $xo_path]
