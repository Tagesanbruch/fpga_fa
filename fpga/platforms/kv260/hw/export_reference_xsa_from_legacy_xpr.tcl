set script_dir [file dirname [file normalize [info script]]]

set legacy_xpr_default "/3.2T/work/flash_attn/kv260-fa/kv260-fa.xpr"
set legacy_xpr [expr {[llength $argv] >= 1 ? [file normalize [lindex $argv 0]] : $legacy_xpr_default}]
set out_dir [expr {[llength $argv] >= 2 ? [file normalize [lindex $argv 1]] : [file join $script_dir generated]}]
set run_name [expr {[llength $argv] >= 3 ? [lindex $argv 2] : "synth_2"}]
set xsa_name [expr {[llength $argv] >= 4 ? [lindex $argv 3] : "kv260_legacy_reference.xsa"}]

file mkdir $out_dir

if {![file exists $legacy_xpr]} {
  puts stderr "[ERR] legacy XPR not found: $legacy_xpr"
  exit 1
}

puts "[INFO] Opening legacy project: $legacy_xpr"
open_project $legacy_xpr

if {[llength [get_runs -quiet $run_name]] == 0} {
  puts stderr "[ERR] requested run not found: $run_name"
  close_project
  exit 1
}

puts "[INFO] Opening run: $run_name"
open_run $run_name

set out_xsa [file join $out_dir $xsa_name]
puts "[INFO] Exporting reference XSA to: $out_xsa"
write_hw_platform -fixed -force -file $out_xsa

puts "[INFO] Done. This XSA is for reference/inspection and may contain fixed PL content from the legacy design."
close_project
exit
