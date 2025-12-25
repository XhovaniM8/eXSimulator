# FPGA Build Makefile for Arty Z7-10
PART := xc7z010clg400-1
TOP := top
PROJECT := exsim_fpga

VIVADO := vivado
VIVADO_FLAGS := -mode batch -source

RTL_DIR := rtl
TB_DIR := tb
BUILD_DIR := build
BIT_FILE := $(BUILD_DIR)/$(PROJECT).bit

RTL_SRCS := $(wildcard $(RTL_DIR)/*.sv) $(wildcard $(RTL_DIR)/*.v)
TB_SRCS := $(wildcard $(TB_DIR)/*.py)

.PHONY: all synth impl bit program sim clean

all: bit

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run synthesis
synth: $(BUILD_DIR)
	$(VIVADO) $(VIVADO_FLAGS) scripts/synth.tcl -tclargs $(PART) $(TOP) $(PROJECT)

# Run implementation (place & route)
impl: synth
	$(VIVADO) $(VIVADO_FLAGS) scripts/impl.tcl -tclargs $(PROJECT)

# Generate bitstream
bit: impl
	$(VIVADO) $(VIVADO_FLAGS) scripts/bit.tcl -tclargs $(PROJECT)

# Program FPGA
program: bit
	$(VIVADO) $(VIVADO_FLAGS) scripts/program.tcl -tclargs $(BIT_FILE)

# Run cocotb simulation
sim:
	cd $(TB_DIR) && make

# Run specific test
test_%:
	cd $(TB_DIR) && make MODULE=test_$*

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(TB_DIR)/__pycache__
	rm -rf $(TB_DIR)/sim_build
	rm -f *.jou *.log
	rm -rf .Xil

# Open Vivado GUI with project
gui:
	$(VIVADO) $(BUILD_DIR)/$(PROJECT).xpr &

# Resource utilization report
util: impl
	@echo "=== Resource Utilization ==="
	@grep -A 20 "Slice Logic" $(BUILD_DIR)/reports/utilization.rpt

# Timing report
timing: impl
	@echo "=== Timing Summary ==="
	@grep -A 10 "Design Timing Summary" $(BUILD_DIR)/reports/timing.rpt
