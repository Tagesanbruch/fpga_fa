set script_dir [file dirname [file normalize [info script]]]
set xo_path [expr {[llength $argv] >= 1 ? [file normalize [lindex $argv 0]] : [file join $script_dir build fa_attention_kernel.xo]}]
set part_name [expr {[llength $argv] >= 2 ? [lindex $argv 1] : "xck26-sfvc784-2LV-c"}]
set clock_ns [expr {[llength $argv] >= 3 ? [lindex $argv 2] : "5.0"}]

set common_dir [file normalize [file join $script_dir .. .. common]]

open_project -reset [file join $script_dir build fa_attention_kernel_hls]
set_top fa_attention_kernel
add_files [file join $script_dir fa_attention_kernel.cpp]
add_files [file join $common_dir fa_q8_8_attention.cpp]

open_solution -reset solution1 -flow_target vitis
set_part $part_name
create_clock -period $clock_ns -name default
csynth_design
export_design -format xo -xo_path $xo_path -kernel fa_attention_kernel
exit
