`timescale 1 ns / 1 ps

module rtl_add_seq_kernel #(
    parameter C_S_AXI_CONTROL_DATA_WIDTH = 32,
    parameter C_S_AXI_CONTROL_ADDR_WIDTH = 6,
    parameter C_M_AXI_GMEM0_ID_WIDTH = 1,
    parameter C_M_AXI_GMEM0_ADDR_WIDTH = 64,
    parameter C_M_AXI_GMEM0_DATA_WIDTH = 32,
    parameter C_M_AXI_GMEM0_AWUSER_WIDTH = 1,
    parameter C_M_AXI_GMEM0_ARUSER_WIDTH = 1,
    parameter C_M_AXI_GMEM0_WUSER_WIDTH  = 1,
    parameter C_M_AXI_GMEM0_RUSER_WIDTH  = 1,
    parameter C_M_AXI_GMEM0_BUSER_WIDTH  = 1,
    parameter C_M_AXI_GMEM1_ID_WIDTH = 1,
    parameter C_M_AXI_GMEM1_ADDR_WIDTH = 64,
    parameter C_M_AXI_GMEM1_DATA_WIDTH = 32,
    parameter C_M_AXI_GMEM1_AWUSER_WIDTH = 1,
    parameter C_M_AXI_GMEM1_ARUSER_WIDTH = 1,
    parameter C_M_AXI_GMEM1_WUSER_WIDTH  = 1,
    parameter C_M_AXI_GMEM1_RUSER_WIDTH  = 1,
    parameter C_M_AXI_GMEM1_BUSER_WIDTH  = 1
) (
    input  wire ap_clk,
    input  wire ap_rst_n,

    output wire m_axi_gmem0_AWVALID,
    input  wire m_axi_gmem0_AWREADY,
    output wire [C_M_AXI_GMEM0_ADDR_WIDTH-1:0] m_axi_gmem0_AWADDR,
    output wire [C_M_AXI_GMEM0_ID_WIDTH-1:0]   m_axi_gmem0_AWID,
    output wire [7:0]                          m_axi_gmem0_AWLEN,
    output wire [2:0]                          m_axi_gmem0_AWSIZE,
    output wire [1:0]                          m_axi_gmem0_AWBURST,
    output wire [1:0]                          m_axi_gmem0_AWLOCK,
    output wire [3:0]                          m_axi_gmem0_AWCACHE,
    output wire [2:0]                          m_axi_gmem0_AWPROT,
    output wire [3:0]                          m_axi_gmem0_AWQOS,
    output wire [3:0]                          m_axi_gmem0_AWREGION,
    output wire [C_M_AXI_GMEM0_AWUSER_WIDTH-1:0] m_axi_gmem0_AWUSER,
    output wire m_axi_gmem0_WVALID,
    input  wire m_axi_gmem0_WREADY,
    output wire [C_M_AXI_GMEM0_DATA_WIDTH-1:0] m_axi_gmem0_WDATA,
    output wire [C_M_AXI_GMEM0_DATA_WIDTH/8-1:0] m_axi_gmem0_WSTRB,
    output wire m_axi_gmem0_WLAST,
    output wire [C_M_AXI_GMEM0_ID_WIDTH-1:0]   m_axi_gmem0_WID,
    output wire [C_M_AXI_GMEM0_WUSER_WIDTH-1:0] m_axi_gmem0_WUSER,
    output reg  m_axi_gmem0_ARVALID,
    input  wire m_axi_gmem0_ARREADY,
    output reg  [C_M_AXI_GMEM0_ADDR_WIDTH-1:0] m_axi_gmem0_ARADDR,
    output wire [C_M_AXI_GMEM0_ID_WIDTH-1:0]   m_axi_gmem0_ARID,
    output wire [7:0]                          m_axi_gmem0_ARLEN,
    output wire [2:0]                          m_axi_gmem0_ARSIZE,
    output wire [1:0]                          m_axi_gmem0_ARBURST,
    output wire [1:0]                          m_axi_gmem0_ARLOCK,
    output wire [3:0]                          m_axi_gmem0_ARCACHE,
    output wire [2:0]                          m_axi_gmem0_ARPROT,
    output wire [3:0]                          m_axi_gmem0_ARQOS,
    output wire [3:0]                          m_axi_gmem0_ARREGION,
    output wire [C_M_AXI_GMEM0_ARUSER_WIDTH-1:0] m_axi_gmem0_ARUSER,
    input  wire m_axi_gmem0_RVALID,
    output reg  m_axi_gmem0_RREADY,
    input  wire [C_M_AXI_GMEM0_DATA_WIDTH-1:0] m_axi_gmem0_RDATA,
    input  wire m_axi_gmem0_RLAST,
    input  wire [C_M_AXI_GMEM0_ID_WIDTH-1:0]   m_axi_gmem0_RID,
    input  wire [C_M_AXI_GMEM0_RUSER_WIDTH-1:0] m_axi_gmem0_RUSER,
    input  wire [1:0]                          m_axi_gmem0_RRESP,
    input  wire m_axi_gmem0_BVALID,
    output wire m_axi_gmem0_BREADY,
    input  wire [1:0]                          m_axi_gmem0_BRESP,
    input  wire [C_M_AXI_GMEM0_ID_WIDTH-1:0]   m_axi_gmem0_BID,
    input  wire [C_M_AXI_GMEM0_BUSER_WIDTH-1:0] m_axi_gmem0_BUSER,

    output reg  m_axi_gmem1_AWVALID,
    input  wire m_axi_gmem1_AWREADY,
    output reg  [C_M_AXI_GMEM1_ADDR_WIDTH-1:0] m_axi_gmem1_AWADDR,
    output wire [C_M_AXI_GMEM1_ID_WIDTH-1:0]   m_axi_gmem1_AWID,
    output wire [7:0]                          m_axi_gmem1_AWLEN,
    output wire [2:0]                          m_axi_gmem1_AWSIZE,
    output wire [1:0]                          m_axi_gmem1_AWBURST,
    output wire [1:0]                          m_axi_gmem1_AWLOCK,
    output wire [3:0]                          m_axi_gmem1_AWCACHE,
    output wire [2:0]                          m_axi_gmem1_AWPROT,
    output wire [3:0]                          m_axi_gmem1_AWQOS,
    output wire [3:0]                          m_axi_gmem1_AWREGION,
    output wire [C_M_AXI_GMEM1_AWUSER_WIDTH-1:0] m_axi_gmem1_AWUSER,
    output reg  m_axi_gmem1_WVALID,
    input  wire m_axi_gmem1_WREADY,
    output reg  [C_M_AXI_GMEM1_DATA_WIDTH-1:0] m_axi_gmem1_WDATA,
    output wire [C_M_AXI_GMEM1_DATA_WIDTH/8-1:0] m_axi_gmem1_WSTRB,
    output wire m_axi_gmem1_WLAST,
    output wire [C_M_AXI_GMEM1_ID_WIDTH-1:0]   m_axi_gmem1_WID,
    output wire [C_M_AXI_GMEM1_WUSER_WIDTH-1:0] m_axi_gmem1_WUSER,
    output wire m_axi_gmem1_ARVALID,
    input  wire m_axi_gmem1_ARREADY,
    output wire [C_M_AXI_GMEM1_ADDR_WIDTH-1:0] m_axi_gmem1_ARADDR,
    output wire [C_M_AXI_GMEM1_ID_WIDTH-1:0]   m_axi_gmem1_ARID,
    output wire [7:0]                          m_axi_gmem1_ARLEN,
    output wire [2:0]                          m_axi_gmem1_ARSIZE,
    output wire [1:0]                          m_axi_gmem1_ARBURST,
    output wire [1:0]                          m_axi_gmem1_ARLOCK,
    output wire [3:0]                          m_axi_gmem1_ARCACHE,
    output wire [2:0]                          m_axi_gmem1_ARPROT,
    output wire [3:0]                          m_axi_gmem1_ARQOS,
    output wire [3:0]                          m_axi_gmem1_ARREGION,
    output wire [C_M_AXI_GMEM1_ARUSER_WIDTH-1:0] m_axi_gmem1_ARUSER,
    input  wire m_axi_gmem1_RVALID,
    output wire m_axi_gmem1_RREADY,
    input  wire [C_M_AXI_GMEM1_DATA_WIDTH-1:0] m_axi_gmem1_RDATA,
    input  wire m_axi_gmem1_RLAST,
    input  wire [C_M_AXI_GMEM1_ID_WIDTH-1:0]   m_axi_gmem1_RID,
    input  wire [C_M_AXI_GMEM1_RUSER_WIDTH-1:0] m_axi_gmem1_RUSER,
    input  wire [1:0]                          m_axi_gmem1_RRESP,
    input  wire m_axi_gmem1_BVALID,
    output reg  m_axi_gmem1_BREADY,
    input  wire [1:0]                          m_axi_gmem1_BRESP,
    input  wire [C_M_AXI_GMEM1_ID_WIDTH-1:0]   m_axi_gmem1_BID,
    input  wire [C_M_AXI_GMEM1_BUSER_WIDTH-1:0] m_axi_gmem1_BUSER,

    input  wire s_axi_control_AWVALID,
    output wire s_axi_control_AWREADY,
    input  wire [C_S_AXI_CONTROL_ADDR_WIDTH-1:0] s_axi_control_AWADDR,
    input  wire s_axi_control_WVALID,
    output wire s_axi_control_WREADY,
    input  wire [C_S_AXI_CONTROL_DATA_WIDTH-1:0] s_axi_control_WDATA,
    input  wire [C_S_AXI_CONTROL_DATA_WIDTH/8-1:0] s_axi_control_WSTRB,
    input  wire s_axi_control_ARVALID,
    output wire s_axi_control_ARREADY,
    input  wire [C_S_AXI_CONTROL_ADDR_WIDTH-1:0] s_axi_control_ARADDR,
    output wire s_axi_control_RVALID,
    input  wire s_axi_control_RREADY,
    output wire [C_S_AXI_CONTROL_DATA_WIDTH-1:0] s_axi_control_RDATA,
    output wire [1:0] s_axi_control_RRESP,
    output wire s_axi_control_BVALID,
    input  wire s_axi_control_BREADY,
    output wire [1:0] s_axi_control_BRESP,
    output wire interrupt
);

localparam ST_IDLE      = 3'd0;
localparam ST_AR        = 3'd1;
localparam ST_R         = 3'd2;
localparam ST_AW_W      = 3'd3;
localparam ST_B         = 3'd4;
localparam ST_DONE      = 3'd5;

reg [2:0] state;
reg [63:0] src_addr_q;
reg [63:0] dst_addr_q;
reg [31:0] remaining_q;
reg [31:0] index_q;
reg [31:0] read_data_q;
reg aw_seen_q;
reg w_seen_q;
reg ap_done_q;
reg ap_ready_q;
reg ap_idle_q;

wire [63:0] in_r;
wire [63:0] out_r;
wire [31:0] length_r;
wire [31:0] base_add;
wire ap_start;
wire ap_continue;

assign m_axi_gmem0_AWVALID  = 1'b0;
assign m_axi_gmem0_AWADDR   = {C_M_AXI_GMEM0_ADDR_WIDTH{1'b0}};
assign m_axi_gmem0_AWID     = {C_M_AXI_GMEM0_ID_WIDTH{1'b0}};
assign m_axi_gmem0_AWLEN    = 8'd0;
assign m_axi_gmem0_AWSIZE   = 3'd2;
assign m_axi_gmem0_AWBURST  = 2'b01;
assign m_axi_gmem0_AWLOCK   = 2'b00;
assign m_axi_gmem0_AWCACHE  = 4'b0011;
assign m_axi_gmem0_AWPROT   = 3'b000;
assign m_axi_gmem0_AWQOS    = 4'b0000;
assign m_axi_gmem0_AWREGION = 4'b0000;
assign m_axi_gmem0_AWUSER   = {C_M_AXI_GMEM0_AWUSER_WIDTH{1'b0}};
assign m_axi_gmem0_WVALID   = 1'b0;
assign m_axi_gmem0_WDATA    = {C_M_AXI_GMEM0_DATA_WIDTH{1'b0}};
assign m_axi_gmem0_WSTRB    = {C_M_AXI_GMEM0_DATA_WIDTH/8{1'b0}};
assign m_axi_gmem0_WLAST    = 1'b0;
assign m_axi_gmem0_WID      = {C_M_AXI_GMEM0_ID_WIDTH{1'b0}};
assign m_axi_gmem0_WUSER    = {C_M_AXI_GMEM0_WUSER_WIDTH{1'b0}};
assign m_axi_gmem0_ARID     = {C_M_AXI_GMEM0_ID_WIDTH{1'b0}};
assign m_axi_gmem0_ARLEN    = 8'd0;
assign m_axi_gmem0_ARSIZE   = 3'd2;
assign m_axi_gmem0_ARBURST  = 2'b01;
assign m_axi_gmem0_ARLOCK   = 2'b00;
assign m_axi_gmem0_ARCACHE  = 4'b0011;
assign m_axi_gmem0_ARPROT   = 3'b000;
assign m_axi_gmem0_ARQOS    = 4'b0000;
assign m_axi_gmem0_ARREGION = 4'b0000;
assign m_axi_gmem0_ARUSER   = {C_M_AXI_GMEM0_ARUSER_WIDTH{1'b0}};
assign m_axi_gmem0_BREADY   = 1'b0;

assign m_axi_gmem1_AWID     = {C_M_AXI_GMEM1_ID_WIDTH{1'b0}};
assign m_axi_gmem1_AWLEN    = 8'd0;
assign m_axi_gmem1_AWSIZE   = 3'd2;
assign m_axi_gmem1_AWBURST  = 2'b01;
assign m_axi_gmem1_AWLOCK   = 2'b00;
assign m_axi_gmem1_AWCACHE  = 4'b0011;
assign m_axi_gmem1_AWPROT   = 3'b000;
assign m_axi_gmem1_AWQOS    = 4'b0000;
assign m_axi_gmem1_AWREGION = 4'b0000;
assign m_axi_gmem1_AWUSER   = {C_M_AXI_GMEM1_AWUSER_WIDTH{1'b0}};
assign m_axi_gmem1_WSTRB    = {C_M_AXI_GMEM1_DATA_WIDTH/8{1'b1}};
assign m_axi_gmem1_WLAST    = 1'b1;
assign m_axi_gmem1_WID      = {C_M_AXI_GMEM1_ID_WIDTH{1'b0}};
assign m_axi_gmem1_WUSER    = {C_M_AXI_GMEM1_WUSER_WIDTH{1'b0}};
assign m_axi_gmem1_ARVALID  = 1'b0;
assign m_axi_gmem1_ARADDR   = {C_M_AXI_GMEM1_ADDR_WIDTH{1'b0}};
assign m_axi_gmem1_ARID     = {C_M_AXI_GMEM1_ID_WIDTH{1'b0}};
assign m_axi_gmem1_ARLEN    = 8'd0;
assign m_axi_gmem1_ARSIZE   = 3'd2;
assign m_axi_gmem1_ARBURST  = 2'b01;
assign m_axi_gmem1_ARLOCK   = 2'b00;
assign m_axi_gmem1_ARCACHE  = 4'b0011;
assign m_axi_gmem1_ARPROT   = 3'b000;
assign m_axi_gmem1_ARQOS    = 4'b0000;
assign m_axi_gmem1_ARREGION = 4'b0000;
assign m_axi_gmem1_ARUSER   = {C_M_AXI_GMEM1_ARUSER_WIDTH{1'b0}};
assign m_axi_gmem1_RREADY   = 1'b0;

rtl_add_seq_kernel_control_s_axi #(
    .C_S_AXI_ADDR_WIDTH(C_S_AXI_CONTROL_ADDR_WIDTH),
    .C_S_AXI_DATA_WIDTH(C_S_AXI_CONTROL_DATA_WIDTH)
) control_if (
    .ACLK(ap_clk),
    .ARESET(~ap_rst_n),
    .ACLK_EN(1'b1),
    .AWADDR(s_axi_control_AWADDR),
    .AWVALID(s_axi_control_AWVALID),
    .AWREADY(s_axi_control_AWREADY),
    .WDATA(s_axi_control_WDATA),
    .WSTRB(s_axi_control_WSTRB),
    .WVALID(s_axi_control_WVALID),
    .WREADY(s_axi_control_WREADY),
    .BRESP(s_axi_control_BRESP),
    .BVALID(s_axi_control_BVALID),
    .BREADY(s_axi_control_BREADY),
    .ARADDR(s_axi_control_ARADDR),
    .ARVALID(s_axi_control_ARVALID),
    .ARREADY(s_axi_control_ARREADY),
    .RDATA(s_axi_control_RDATA),
    .RRESP(s_axi_control_RRESP),
    .RVALID(s_axi_control_RVALID),
    .RREADY(s_axi_control_RREADY),
    .interrupt(interrupt),
    .in_r(in_r),
    .out_r(out_r),
    .length_r(length_r),
    .base_add(base_add),
    .ap_start(ap_start),
    .ap_done(ap_done_q),
    .ap_ready(ap_ready_q),
    .ap_continue(ap_continue),
    .ap_idle(ap_idle_q)
);

always @(posedge ap_clk) begin
    if (!ap_rst_n) begin
        state <= ST_IDLE;
        src_addr_q <= 64'd0;
        dst_addr_q <= 64'd0;
        remaining_q <= 32'd0;
        index_q <= 32'd0;
        read_data_q <= 32'd0;
        aw_seen_q <= 1'b0;
        w_seen_q <= 1'b0;
        ap_done_q <= 1'b0;
        ap_ready_q <= 1'b0;
        ap_idle_q <= 1'b1;
        m_axi_gmem0_ARVALID <= 1'b0;
        m_axi_gmem0_ARADDR <= {C_M_AXI_GMEM0_ADDR_WIDTH{1'b0}};
        m_axi_gmem0_RREADY <= 1'b0;
        m_axi_gmem1_AWVALID <= 1'b0;
        m_axi_gmem1_AWADDR <= {C_M_AXI_GMEM1_ADDR_WIDTH{1'b0}};
        m_axi_gmem1_WVALID <= 1'b0;
        m_axi_gmem1_WDATA <= {C_M_AXI_GMEM1_DATA_WIDTH{1'b0}};
        m_axi_gmem1_BREADY <= 1'b0;
    end else begin
        ap_done_q <= 1'b0;
        ap_ready_q <= 1'b0;

        case (state)
            ST_IDLE: begin
                ap_idle_q <= 1'b1;
                m_axi_gmem0_ARVALID <= 1'b0;
                m_axi_gmem0_RREADY <= 1'b0;
                m_axi_gmem1_AWVALID <= 1'b0;
                m_axi_gmem1_WVALID <= 1'b0;
                m_axi_gmem1_BREADY <= 1'b0;
                aw_seen_q <= 1'b0;
                w_seen_q <= 1'b0;
                if (ap_start) begin
                    src_addr_q <= in_r;
                    dst_addr_q <= out_r;
                    remaining_q <= length_r;
                    index_q <= 32'd0;
                    ap_idle_q <= 1'b0;
                    if (length_r == 32'd0) begin
                        state <= ST_DONE;
                    end else begin
                        state <= ST_AR;
                    end
                end
            end

            ST_AR: begin
                m_axi_gmem0_ARVALID <= 1'b1;
                m_axi_gmem0_ARADDR <= src_addr_q;
                if (m_axi_gmem0_ARVALID && m_axi_gmem0_ARREADY) begin
                    m_axi_gmem0_ARVALID <= 1'b0;
                    m_axi_gmem0_RREADY <= 1'b1;
                    state <= ST_R;
                end
            end

            ST_R: begin
                if (m_axi_gmem0_RVALID && m_axi_gmem0_RREADY) begin
                    read_data_q <= m_axi_gmem0_RDATA;
                    m_axi_gmem0_RREADY <= 1'b0;
                    m_axi_gmem1_AWADDR <= dst_addr_q;
                    m_axi_gmem1_WDATA <= m_axi_gmem0_RDATA + base_add + index_q;
                    m_axi_gmem1_AWVALID <= 1'b1;
                    m_axi_gmem1_WVALID <= 1'b1;
                    aw_seen_q <= 1'b0;
                    w_seen_q <= 1'b0;
                    state <= ST_AW_W;
                end
            end

            ST_AW_W: begin
                if (m_axi_gmem1_AWVALID && m_axi_gmem1_AWREADY) begin
                    m_axi_gmem1_AWVALID <= 1'b0;
                    aw_seen_q <= 1'b1;
                end
                if (m_axi_gmem1_WVALID && m_axi_gmem1_WREADY) begin
                    m_axi_gmem1_WVALID <= 1'b0;
                    w_seen_q <= 1'b1;
                end
                if ((aw_seen_q || (m_axi_gmem1_AWVALID && m_axi_gmem1_AWREADY)) &&
                    (w_seen_q || (m_axi_gmem1_WVALID && m_axi_gmem1_WREADY))) begin
                    m_axi_gmem1_BREADY <= 1'b1;
                    state <= ST_B;
                end
            end

            ST_B: begin
                if (m_axi_gmem1_BVALID && m_axi_gmem1_BREADY) begin
                    m_axi_gmem1_BREADY <= 1'b0;
                    src_addr_q <= src_addr_q + 64'd4;
                    dst_addr_q <= dst_addr_q + 64'd4;
                    remaining_q <= remaining_q - 32'd1;
                    index_q <= index_q + 32'd1;
                    if (remaining_q == 32'd1) begin
                        state <= ST_DONE;
                    end else begin
                        state <= ST_AR;
                    end
                end
            end

            ST_DONE: begin
                ap_done_q <= 1'b1;
                ap_ready_q <= 1'b1;
                ap_idle_q <= 1'b1;
                if (!ap_start || ap_continue) begin
                    state <= ST_IDLE;
                end
            end

            default: begin
                state <= ST_IDLE;
            end
        endcase
    end
end

endmodule
