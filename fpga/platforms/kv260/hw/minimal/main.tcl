set proj_name kv260_min_shell
set proj_dir ./project
set proj_board [get_board_parts "*:kv260_som:*" -latest_file_version]
set bd_tcl_dir ./scripts
set jobs 8

for { set i 0 } { $i < $argc } { incr i } {
  if { [lindex $argv $i] == "-jobs" } {
    incr i
    set jobs [lindex $argv $i]
  }
}

create_project -name $proj_name -force -dir $proj_dir -part [get_property PART_NAME [get_board_parts $proj_board]]
set_property board_part $proj_board [current_project]
set_property board_connections {som240_1_connector xilinx.com:kv260_carrier:som240_1_connector:1.3} [current_project]

create_bd_design $proj_name
current_bd_design $proj_name

set parentCell [get_bd_cells /]
set parentObj [get_bd_cells $parentCell]
current_bd_instance $parentObj

source $bd_tcl_dir/config_bd.tcl
save_bd_design

make_wrapper -files [get_files $proj_dir/${proj_name}.srcs/sources_1/bd/$proj_name/${proj_name}.bd] -top
import_files -force -norecurse $proj_dir/${proj_name}.srcs/sources_1/bd/$proj_name/hdl/${proj_name}_wrapper.v
update_compile_order
set_property top ${proj_name}_wrapper [current_fileset]
update_compile_order -fileset sources_1

set_property platform.board_id $proj_name [current_project]
set_property platform.default_output_type "xclbin" [current_project]
set_property platform.design_intent.datacenter false [current_project]
set_property platform.design_intent.embedded true [current_project]
set_property platform.design_intent.external_host false [current_project]
set_property platform.design_intent.server_managed false [current_project]
set_property platform.extensible true [current_project]
set_property platform.platform_state "pre_synth" [current_project]
set_property platform.name $proj_name [current_project]
set_property platform.vendor "openai-local" [current_project]
set_property platform.version "1.0" [current_project]

set_property synth_checkpoint_mode Hierarchical [get_files $proj_dir/${proj_name}.srcs/sources_1/bd/$proj_name/${proj_name}.bd]

launch_runs synth_1 -jobs $jobs
wait_on_run synth_1

launch_runs impl_1 -to_step write_bitstream -jobs $jobs
wait_on_run impl_1

write_hw_platform -force -include_bit -file $proj_dir/${proj_name}.xsa
validate_hw_platform -verbose $proj_dir/${proj_name}.xsa

exit
