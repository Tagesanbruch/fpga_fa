set script_dir [file dirname [file normalize [info script]]]
set repo_root [file normalize [file join $script_dir .. .. .. ..]]

set legacy_xpr_default "/3.2T/work/flash_attn/kv260-fa/kv260-fa.xpr"
set legacy_xpr [expr {[llength $argv] >= 1 ? [file normalize [lindex $argv 0]] : $legacy_xpr_default}]
set out_dir [expr {[llength $argv] >= 2 ? [file normalize [lindex $argv 1]] : [file join $script_dir generated]}]
set bd_name [expr {[llength $argv] >= 3 ? [lindex $argv 2] : "design_fa"}]

file mkdir $out_dir

if {![file exists $legacy_xpr]} {
  puts stderr "[ERR] legacy XPR not found: $legacy_xpr"
  exit 1
}

puts "[INFO] Opening legacy project: $legacy_xpr"
open_project $legacy_xpr

set bd_files [get_files -quiet */${bd_name}.bd]
if {[llength $bd_files] == 0} {
  puts stderr "[ERR] Block design ${bd_name}.bd not found in project"
  close_project
  exit 1
}

set bd_file [lindex $bd_files 0]
open_bd_design $bd_file

set out_tcl [file join $out_dir ${bd_name}_legacy_reference.tcl]
puts "[INFO] Writing BD Tcl to: $out_tcl"
write_bd_tcl -force $out_tcl

puts "[INFO] Done. This Tcl is for reference extraction, not yet the final minimal platform shell."
close_project
exit
