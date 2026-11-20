// ALU escalar de 32 bits: calcula direcciones, contadores de bucle y comparaciones de saltos.
`include "vproc_defs.vh"

module vproc_alu (
    input  wire [2:0]  op,
    input  wire [31:0] a,
    input  wire [31:0] b,
    output reg  [31:0] y,
    output wire        eq,   // a == b, para BNE
    output wire        lt    // a < b con signo, para BLT
);
    assign eq = (a == b);
    assign lt = ($signed(a) < $signed(b));

    always @(*) begin
        case (op)
            `ALU_ADD:   y = a + b;
            `ALU_SUB:   y = a - b;
            `ALU_MUL:   y = a * b;
            `ALU_SLL:   y = a << b[4:0];
            `ALU_PASSB: y = b;
            default:    y = 32'd0;
        endcase
    end
endmodule
