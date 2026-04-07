set script_dir [file dirname [file normalize [info script]]]
set args $argv
if {[llength $args] > 0 && [string match "*.tcl" [lindex $args 0]]} {
  set args [lrange $args 1 end]
}

set xo_path [expr {[llength $args] >= 1 ? [file normalize [lindex $args 0]] : [file join $script_dir build add_seq_kernel.xo]}]
set part_name [expr {[llength $args] >= 2 ? [lindex $args 1] : "xck26-sfvc784-2LV-c"}]
set clock_ns [expr {[llength $args] >= 3 ? [lindex $args 2] : "5.0"}]

set build_dir [file join $script_dir build]

file mkdir $build_dir
cd $build_dir

open_project -reset add_seq_kernel_hls
set_top add_seq_kernel
add_files [file join $script_dir add_seq_kernel.cpp]

open_solution -reset solution1 -flow_target vitis
set_part $part_name
create_clock -period $clock_ns -name default
csynth_design
export_design -format xo -output $xo_path
exit
