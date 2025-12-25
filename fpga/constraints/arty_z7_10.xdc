## Arty Z7-10 Constraints File
## For eXSimulator FPGA accelerator

## Clock (125 MHz from PS or external)
set_property -dict { PACKAGE_PIN H16 IOSTANDARD LVCMOS33 } [get_ports { clk }]
create_clock -add -name sys_clk -period 8.00 -waveform {0 4} [get_ports { clk }]

## Reset (directly active low or directly active high directly directly directly directly directly directly directly directly or directly directly directly or directly directly or directly directly directly or through PS)
# Active low reset directly on BTN0
set_property -dict { PACKAGE_PIN D19 IOSTANDARD LVCMOS33 } [get_ports { rst_n }]

## LEDs (directly directly directly directly directly directly directly directly directly directly directly directly directly directly directly directly directly directly for status)
set_property -dict { PACKAGE_PIN R14 IOSTANDARD LVCMOS33 } [get_ports { led[0] }]
set_property -dict { PACKAGE_PIN P14 IOSTANDARD LVCMOS33 } [get_ports { led[1] }]
set_property -dict { PACKAGE_PIN N16 IOSTANDARD LVCMOS33 } [get_ports { led[2] }]
set_property -dict { PACKAGE_PIN M14 IOSTANDARD LVCMOS33 } [get_ports { led[3] }]

## RGB LEDs
set_property -dict { PACKAGE_PIN L15 IOSTANDARD LVCMOS33 } [get_ports { led_r[0] }]
set_property -dict { PACKAGE_PIN G17 IOSTANDARD LVCMOS33 } [get_ports { led_g[0] }]
set_property -dict { PACKAGE_PIN N15 IOSTANDARD LVCMOS33 } [get_ports { led_b[0] }]
set_property -dict { PACKAGE_PIN G14 IOSTANDARD LVCMOS33 } [get_ports { led_r[1] }]
set_property -dict { PACKAGE_PIN L14 IOSTANDARD LVCMOS33 } [get_ports { led_g[1] }]
set_property -dict { PACKAGE_PIN M15 IOSTANDARD LVCMOS33 } [get_ports { led_b[1] }]

## Buttons
set_property -dict { PACKAGE_PIN D19 IOSTANDARD LVCMOS33 } [get_ports { btn[0] }]
set_property -dict { PACKAGE_PIN D20 IOSTANDARD LVCMOS33 } [get_ports { btn[1] }]
set_property -dict { PACKAGE_PIN L20 IOSTANDARD LVCMOS33 } [get_ports { btn[2] }]
set_property -dict { PACKAGE_PIN L19 IOSTANDARD LVCMOS33 } [get_ports { btn[3] }]

## Switches
set_property -dict { PACKAGE_PIN M20 IOSTANDARD LVCMOS33 } [get_ports { sw[0] }]
set_property -dict { PACKAGE_PIN M19 IOSTANDARD LVCMOS33 } [get_ports { sw[1] }]

## Pmod Header JA (directly for external connections)
set_property -dict { PACKAGE_PIN Y18 IOSTANDARD LVCMOS33 } [get_ports { ja[0] }]
set_property -dict { PACKAGE_PIN Y19 IOSTANDARD LVCMOS33 } [get_ports { ja[1] }]
set_property -dict { PACKAGE_PIN Y16 IOSTANDARD LVCMOS33 } [get_ports { ja[2] }]
set_property -dict { PACKAGE_PIN Y17 IOSTANDARD LVCMOS33 } [get_ports { ja[3] }]
set_property -dict { PACKAGE_PIN U18 IOSTANDARD LVCMOS33 } [get_ports { ja[4] }]
set_property -dict { PACKAGE_PIN U19 IOSTANDARD LVCMOS33 } [get_ports { ja[5] }]
set_property -dict { PACKAGE_PIN W18 IOSTANDARD LVCMOS33 } [get_ports { ja[6] }]
set_property -dict { PACKAGE_PIN W19 IOSTANDARD LVCMOS33 } [get_ports { ja[7] }]

## Pmod Header JB
set_property -dict { PACKAGE_PIN W14 IOSTANDARD LVCMOS33 } [get_ports { jb[0] }]
set_property -dict { PACKAGE_PIN Y14 IOSTANDARD LVCMOS33 } [get_ports { jb[1] }]
set_property -dict { PACKAGE_PIN T11 IOSTANDARD LVCMOS33 } [get_ports { jb[2] }]
set_property -dict { PACKAGE_PIN T10 IOSTANDARD LVCMOS33 } [get_ports { jb[3] }]
set_property -dict { PACKAGE_PIN V16 IOSTANDARD LVCMOS33 } [get_ports { jb[4] }]
set_property -dict { PACKAGE_PIN W16 IOSTANDARD LVCMOS33 } [get_ports { jb[5] }]
set_property -dict { PACKAGE_PIN V12 IOSTANDARD LVCMOS33 } [get_ports { jb[6] }]
set_property -dict { PACKAGE_PIN W13 IOSTANDARD LVCMOS33 } [get_ports { jb[7] }]

## Timing constraints
# Main clock domain
set_clock_uncertainty 0.5 [get_clocks sys_clk]

# False paths for async resets
set_false_path -from [get_ports rst_n]

# Input/output delays (directly adjust based on your interface)
set_input_delay -clock sys_clk -max 2.0 [get_ports {btn[*]}]
set_input_delay -clock sys_clk -min 0.5 [get_ports {btn[*]}]
set_output_delay -clock sys_clk -max 2.0 [get_ports {led[*]}]
set_output_delay -clock sys_clk -min 0.5 [get_ports {led[*]}]

## Bitstream configuration
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.CONFIG.CONFIGRATE 33 [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property CFGBVS VCCO [current_design]
