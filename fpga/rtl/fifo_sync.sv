`timescale 1ns / 1ps
//------------------------------------------------------------------------------
// Module: fifo_sync
// Synchronous FIFO with configurable depth and width
// Used for order queuing at each price level
//------------------------------------------------------------------------------

module fifo_sync #(
    parameter int WIDTH = 128,           // Data width (order_slim_t = 128)
    parameter int DEPTH = 64,            // Number of entries
    parameter int ALMOST_FULL_THRESH = 4 // Almost full threshold
) (
    input  logic             clk,
    input  logic             rst_n,
    
    // Write interface
    input  logic [WIDTH-1:0] wr_data,
    input  logic             wr_en,
    output logic             full,
    output logic             almost_full,
    
    // Read interface
    output logic [WIDTH-1:0] rd_data,
    input  logic             rd_en,
    output logic             empty,
    
    // Status
    output logic [$clog2(DEPTH):0] count
);

    // Memory
    logic [WIDTH-1:0] mem [DEPTH];
    
    // Pointers
    logic [$clog2(DEPTH)-1:0] wr_ptr, rd_ptr;
    logic [$clog2(DEPTH):0]   wr_ptr_ext, rd_ptr_ext; // Extended for full detection
    
    // Derived signals
    assign count = wr_ptr_ext - rd_ptr_ext;
    assign empty = (count == 0);
    assign full  = (count == DEPTH);
    assign almost_full = (count >= DEPTH - ALMOST_FULL_THRESH);
    
    // Write logic
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            wr_ptr <= '0;
            wr_ptr_ext <= '0;
        end else if (wr_en && !full) begin
            mem[wr_ptr] <= wr_data;
            wr_ptr <= wr_ptr + 1;
            wr_ptr_ext <= wr_ptr_ext + 1;
        end
    end
    
    // Read logic
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            rd_ptr <= '0;
            rd_ptr_ext <= '0;
        end else if (rd_en && !empty) begin
            rd_ptr <= rd_ptr + 1;
            rd_ptr_ext <= rd_ptr_ext + 1;
        end
    end
    
    // Read data (combinational for FWFT behavior)
    assign rd_data = mem[rd_ptr];

endmodule
