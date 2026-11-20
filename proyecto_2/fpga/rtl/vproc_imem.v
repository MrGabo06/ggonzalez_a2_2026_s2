// Memoria de instrucciones: WORDS palabras de 32 bits, lectura síncrona.
// El host la escribe (programa ensamblado) mientras el núcleo está detenido.
module vproc_imem #(
    parameter WORDS = 512
) (
    input  wire                      clk,
    input  wire [$clog2(WORDS)-1:0]  addr,
    output reg  [31:0]               q,

    input  wire                      host_we,
    input  wire [$clog2(WORDS)-1:0]  host_addr,
    input  wire [31:0]               host_wdata
);
    reg [31:0] mem [0:WORDS-1];

    always @(posedge clk) begin
        if (host_we) mem[host_addr] <= host_wdata;
        q <= mem[addr];
    end
endmodule
