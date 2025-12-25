# cocotb Makefile for FPGA testbenches

# Default simulator (icarus is free, verilator is faster)
SIM ?= icarus
TOPLEVEL_LANG ?= verilog

# Source files
VERILOG_SOURCES = $(PWD)/../rtl/exsim_types_pkg.sv
VERILOG_SOURCES += $(PWD)/../rtl/fifo_sync.sv
# Add more RTL files as needed:
# VERILOG_SOURCES += $(PWD)/../rtl/order_book.sv

# Test module (set via command line: make MODULE=test_fifo)
MODULE ?= test_fifo

# Top-level module for each test
ifeq ($(MODULE),test_fifo)
    TOPLEVEL = fifo_sync
endif

ifeq ($(MODULE),test_order_book)
    TOPLEVEL = order_book
    VERILOG_SOURCES += $(PWD)/../rtl/order_book.sv
endif

# Simulator-specific flags
ifeq ($(SIM),icarus)
    COMPILE_ARGS += -g2012
endif

ifeq ($(SIM),verilator)
    EXTRA_ARGS += --trace --trace-structs
    COMPILE_ARGS += -Wno-fatal
endif

# Include cocotb makefile
include $(shell cocotb-config --makefiles)/Makefile.sim

# Clean simulation artifacts
clean::
	rm -rf sim_build
	rm -rf __pycache__
	rm -f results.xml
	rm -f *.vcd
