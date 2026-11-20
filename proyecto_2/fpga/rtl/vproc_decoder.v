// Decodificador de instrucciones del VPROC.
// Es puramente combinacional: separa los campos de la instrucción y genera las señales
// de control que usa el datapath. Qué ciclo usa cada señal lo decide la unidad de control.
`include "vproc_defs.vh"

module vproc_decoder (
    input  wire [31:0] instr,

    // Campos
    output wire [5:0]  opcode,
    output wire [3:0]  rd,
    output wire [3:0]  rs,
    output wire [3:0]  rt,
    output wire [31:0] imm,          // inmediato de 14 bits extendido con signo

    // Clase de instrucción
    output reg         is_halt,
    output reg         is_branch,    // BNE o BLT
    output reg         is_jump,      // JMP
    output reg         is_vld,
    output reg         is_vst,
    output reg         illegal,

    // Escritura de registros
    output reg         rf_we,        // escribe r[rd] con el resultado escalar
    output reg         vrf_we,       // escribe v[rd] (desde la unidad vectorial)

    // Selección de operaciones
    output reg  [2:0]  alu_op,
    output reg         alu_b_imm,    // operando B de la ALU = imm (si no, r[rt])
    output reg         alu_b_cyc,    // operando B de la ALU = contador de ciclos
    output reg  [2:0]  vu_op,
    output reg         br_lt         // 1: BLT, 0: BNE
);
    assign opcode = instr[31:26];
    assign rd     = instr[25:22];
    assign rs     = instr[21:18];
    assign rt     = instr[17:14];
    assign imm    = {{18{instr[13]}}, instr[13:0]};

    always @(*) begin
        // Valores por defecto: la instrucción no hace nada
        is_halt   = 1'b0;
        is_branch = 1'b0;
        is_jump   = 1'b0;
        is_vld    = 1'b0;
        is_vst    = 1'b0;
        illegal   = 1'b0;
        rf_we     = 1'b0;
        vrf_we    = 1'b0;
        alu_op    = `ALU_ADD;
        alu_b_imm = 1'b0;
        alu_b_cyc = 1'b0;
        vu_op     = `VU_ADD;
        br_lt     = 1'b0;

        case (opcode)
            `OP_NOP:    ;
            `OP_HALT:   is_halt = 1'b1;

            `OP_ADD:    begin rf_we = 1'b1; alu_op = `ALU_ADD; end
            `OP_SUB:    begin rf_we = 1'b1; alu_op = `ALU_SUB; end
            `OP_MUL:    begin rf_we = 1'b1; alu_op = `ALU_MUL; end
            `OP_ADDI:   begin rf_we = 1'b1; alu_op = `ALU_ADD; alu_b_imm = 1'b1; end
            `OP_SLLI:   begin rf_we = 1'b1; alu_op = `ALU_SLL; alu_b_imm = 1'b1; end
            `OP_RDCYC:  begin rf_we = 1'b1; alu_op = `ALU_PASSB; alu_b_cyc = 1'b1; end

            `OP_BNE:    begin is_branch = 1'b1; br_lt = 1'b0; end
            `OP_BLT:    begin is_branch = 1'b1; br_lt = 1'b1; end
            `OP_JMP:    is_jump = 1'b1;

            `OP_VLD:    begin is_vld = 1'b1; vrf_we = 1'b1; end
            `OP_VST:    is_vst = 1'b1;
            `OP_VADD:   begin vrf_we = 1'b1; vu_op = `VU_ADD;   end
            `OP_VSUB:   begin vrf_we = 1'b1; vu_op = `VU_SUB;   end
            `OP_VMULS:  begin vrf_we = 1'b1; vu_op = `VU_MULS;  end
            `OP_VMACS:  begin vrf_we = 1'b1; vu_op = `VU_MACS;  end
            `OP_VADDI:  begin vrf_we = 1'b1; vu_op = `VU_ADDI;  end
            `OP_VSRAI:  begin vrf_we = 1'b1; vu_op = `VU_SRAI;  end
            `OP_VABS:   begin vrf_we = 1'b1; vu_op = `VU_ABS;   end
            `OP_VBCAST: begin vrf_we = 1'b1; vu_op = `VU_BCAST; end

            default:    illegal = 1'b1;
        endcase
    end
endmodule
