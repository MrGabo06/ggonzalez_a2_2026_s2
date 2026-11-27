// Unidad vectorial: LANES carriles idénticos de 32 bits que aplican la misma operación
// en el mismo ciclo (SIMD). Es donde está el paralelismo de datos del acelerador.
//
// Multiplicaciones (VMULS, VMACS): cada carril multiplica sus 18 bits bajos (con signo) por
// los 16 bits bajos del escalar (con signo). Así cada carril usa un solo multiplicador
// 18x18 de la FPGA. Para los filtros alcanza: los productos parciales son < 2^17.
`include "vproc_defs.vh"

module vproc_vector_unit #(
    parameter LANES = 8
) (
    input  wire [2:0]            op,
    input  wire [LANES*32-1:0]   va,     // v[rs]
    input  wire [LANES*32-1:0]   vb,     // v[rt]
    input  wire [LANES*32-1:0]   vacc,   // v[rd] (acumulador de VMACS)
    input  wire [31:0]           scalar, // r[rt] (VMULS, VMACS) o r[rs] (VBCAST)
    input  wire [31:0]           imm,
    output wire [LANES*32-1:0]   vy
);
    genvar i;
    generate
        for (i = 0; i < LANES; i = i + 1) begin : lane
            wire signed [31:0] a   = va[i*32 +: 32];
            wire signed [31:0] b   = vb[i*32 +: 32];
            wire signed [31:0] acc = vacc[i*32 +: 32];
            wire signed [35:0] prod = $signed(a[17:0]) * $signed(scalar[15:0]);
            reg  signed [31:0] r;

            always @(*) begin
                case (op)
                    `VU_ADD:   r = a + b;
                    `VU_SUB:   r = a - b;
                    `VU_MULS:  r = prod[31:0];
                    `VU_MACS:  r = acc + prod[31:0];
                    `VU_ADDI:  r = a + $signed(imm);
                    `VU_SRAI:  r = a >>> imm[4:0];
                    `VU_ABS:   r = (a < 0) ? -a : a;
                    `VU_BCAST: r = scalar;
                    default:   r = 32'sd0;
                endcase
            end

            assign vy[i*32 +: 32] = r;
        end
    endgenerate
endmodule
