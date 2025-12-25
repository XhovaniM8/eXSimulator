# FPGA Hardware Accelerator

Hardware acceleration for latency-critical paths using Arty Z7-10.

## Why FPGA?

Software matching engines hit limits around 1M orders/sec with ~1µs latency. FPGAs can push to:
- **10M+ messages/sec** throughput
- **Sub-100ns** deterministic latency
- **Wire-speed** market data parsing

## Target Accelerations

### Phase 1: Market Data Parser
Offload ITCH/OUCH protocol parsing from CPU to FPGA.

```
Network → FPGA (parse) → PCIe/AXI → CPU (matching logic)
```

**Benefit**: CPU sees clean `Order` structs, no parsing overhead.

### Phase 2: Order Book Hardware
Implement price-level lookup in hardware.

```
Order → FPGA (price lookup, queue insert) → Trade output
```

**Benefit**: O(1) hardware lookup vs O(log n) software tree.

### Phase 3: Full Matching Engine
Complete matching in fabric, CPU only for risk/reporting.

## Directory Structure

```
fpga/
├── rtl/                    # Verilog/SystemVerilog source
│   ├── market_data_parser.sv
│   ├── order_book.sv
│   ├── price_level.sv
│   ├── fifo_sync.sv
│   └── axi_interface.sv
├── tb/                     # Testbenches (cocotb or SV)
│   ├── test_parser.py
│   └── test_order_book.py
├── constraints/            # Arty Z7-10 XDC files
│   └── arty_z7_10.xdc
├── sdk/                    # Zynq PS software (bare-metal or Linux)
│   └── driver/
├── docs/
│   └── architecture.md
├── scripts/
│   ├── build.tcl          # Vivado build script
│   └── program.tcl
└── Makefile
```

## Arty Z7-10 Resources

| Resource | Available | Notes |
|----------|-----------|-------|
| LUTs | 17,600 | Enough for simple order book |
| FFs | 35,200 | State machines, FIFOs |
| BRAM | 50 (36Kb each) | ~225KB for price levels |
| DSP | 80 | Price comparison |
| PS | Dual Cortex-A9 | Run software side |

## Integration with C++ Simulator

The FPGA accelerator shares interfaces with the software version:

```cpp
// include/core/types.hpp - shared between HW and SW
struct Order {
    uint64_t id;
    int64_t price;      // Fixed-point, matches HW representation
    uint32_t quantity;
    uint8_t side;       // 0=buy, 1=sell
    // ... packed to match AXI width
} __attribute__((packed));
```

**AXI-Stream interface** for order ingress:
```systemverilog
module order_ingress (
    input  wire        aclk,
    input  wire        aresetn,
    // AXI-Stream slave (from PS or network)
    input  wire [63:0] s_axis_tdata,
    input  wire        s_axis_tvalid,
    output wire        s_axis_tready,
    input  wire        s_axis_tlast,
    // Internal order interface
    output Order       order_out,
    output wire        order_valid
);
```

## Build (Vivado)

```bash
cd fpga
make synth      # Synthesis
make impl       # Implementation
make bit        # Generate bitstream
make program    # Program FPGA
```

## Simulation

```bash
# cocotb tests
cd fpga/tb
make test_parser
make test_order_book
```

## Development Workflow

1. Develop/test in C++ simulator first
2. Port critical path to SystemVerilog
3. Verify with cocotb against C++ reference
4. Synthesize and benchmark on hardware
5. Compare latency: SW vs HW

## References

- [Low Latency Trading on FPGA](https://www.youtube.com/watch?v=ekMSaJxBEv8)
- [Xilinx Zynq-7000 TRM](https://docs.xilinx.com/v/u/en-US/ug585-Zynq-7000-TRM)
- Your Neucom ADA experience with event cameras translates directly here
