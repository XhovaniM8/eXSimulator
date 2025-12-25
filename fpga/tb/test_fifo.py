"""
cocotb testbench for fifo_sync module
Run with: make MODULE=test_fifo
"""
import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, ClockCycles
from cocotb.handle import Force
import random


async def reset_dut(dut):
    """Reset the DUT"""
    dut.rst_n.value = 0
    dut.wr_en.value = 0
    dut.rd_en.value = 0
    dut.wr_data.value = 0
    await ClockCycles(dut.clk, 5)
    dut.rst_n.value = 1
    await ClockCycles(dut.clk, 2)


async def write_fifo(dut, data):
    """Write a single value to FIFO"""
    dut.wr_data.value = data
    dut.wr_en.value = 1
    await RisingEdge(dut.clk)
    dut.wr_en.value = 0


async def read_fifo(dut):
    """Read a single value from FIFO"""
    dut.rd_en.value = 1
    await RisingEdge(dut.clk)
    dut.rd_en.value = 0
    return dut.rd_data.value


@cocotb.test()
async def test_empty_on_reset(dut):
    """FIFO should be empty after reset"""
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())
    
    await reset_dut(dut)
    
    assert dut.empty.value == 1, "FIFO should be empty after reset"
    assert dut.full.value == 0, "FIFO should not be full after reset"
    assert dut.count.value == 0, "Count should be 0 after reset"


@cocotb.test()
async def test_write_read_single(dut):
    """Write and read a single value"""
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())
    
    await reset_dut(dut)
    
    # Write
    test_val = 0xDEADBEEF
    await write_fifo(dut, test_val)
    await RisingEdge(dut.clk)
    
    assert dut.empty.value == 0, "FIFO should not be empty after write"
    assert dut.count.value == 1, "Count should be 1"
    
    # Read
    read_val = await read_fifo(dut)
    assert read_val == test_val, f"Read {read_val}, expected {test_val}"


@cocotb.test()
async def test_fifo_ordering(dut):
    """FIFO should maintain order (first in, first out)"""
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())
    
    await reset_dut(dut)
    
    # Write multiple values
    test_vals = [i * 100 for i in range(10)]
    for val in test_vals:
        await write_fifo(dut, val)
        await RisingEdge(dut.clk)
    
    # Read and verify order
    for expected in test_vals:
        read_val = await read_fifo(dut)
        await RisingEdge(dut.clk)
        assert read_val == expected, f"Read {read_val}, expected {expected}"


@cocotb.test()
async def test_full_flag(dut):
    """FIFO full flag should assert when full"""
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())
    
    await reset_dut(dut)
    
    # Fill FIFO (default depth = 64)
    for i in range(64):
        assert dut.full.value == 0, f"FIFO should not be full at count {i}"
        await write_fifo(dut, i)
        await RisingEdge(dut.clk)
    
    assert dut.full.value == 1, "FIFO should be full"
    assert dut.count.value == 64, "Count should be 64"


@cocotb.test()
async def test_simultaneous_rw(dut):
    """Simultaneous read and write should work"""
    clock = Clock(dut.clk, 10, units="ns")
    cocotb.start_soon(clock.start())
    
    await reset_dut(dut)
    
    # Pre-fill with some data
    for i in range(10):
        await write_fifo(dut, i)
        await RisingEdge(dut.clk)
    
    initial_count = int(dut.count.value)
    
    # Simultaneous read/write
    dut.wr_data.value = 999
    dut.wr_en.value = 1
    dut.rd_en.value = 1
    await RisingEdge(dut.clk)
    dut.wr_en.value = 0
    dut.rd_en.value = 0
    await RisingEdge(dut.clk)
    
    # Count should remain same
    assert dut.count.value == initial_count, "Count should be unchanged after simultaneous r/w"


@cocotb.test()
async def test_throughput(dut):
    """Measure throughput: write continuously, read continuously"""
    clock = Clock(dut.clk, 10, units="ns")  # 100 MHz
    cocotb.start_soon(clock.start())
    
    await reset_dut(dut)
    
    # Fill half the FIFO
    for i in range(32):
        await write_fifo(dut, i)
        await RisingEdge(dut.clk)
    
    # Continuous read for 100 cycles
    read_count = 0
    for _ in range(100):
        if not dut.empty.value:
            dut.rd_en.value = 1
            read_count += 1
        else:
            dut.rd_en.value = 0
        await RisingEdge(dut.clk)
    
    dut.rd_en.value = 0
    
    # At 100MHz, 32 reads should take 32 cycles minimum
    dut._log.info(f"Read {read_count} entries in 100 cycles")
