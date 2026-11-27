// Banco de memoria de un puerto con lectura síncrona (devuelve el dato anterior al escribir).
// Es un módulo aparte, con la plantilla que reconoce Quartus, para que cada banco se
// implemente en bloques M10K y no en registros. Declarar la memoria dentro de un generate
// hacía que Quartus la sintetizara con flip-flops (más de 130 000 para 16 KB).
module vproc_bram #(
    parameter WIDTH = 8,
    parameter DEPTH = 2048
) (
    input  wire                     clk,
    input  wire                     en,
    input  wire                     we,
    input  wire [$clog2(DEPTH)-1:0] addr,
    input  wire [WIDTH-1:0]         wdata,
    output reg  [WIDTH-1:0]         q
);
    (* ramstyle = "M10K" *) reg [WIDTH-1:0] mem [0:DEPTH-1];

    always @(posedge clk) begin
        if (en) begin
            if (we) mem[addr] <= wdata;
            q <= mem[addr];
        end
    end
endmodule
