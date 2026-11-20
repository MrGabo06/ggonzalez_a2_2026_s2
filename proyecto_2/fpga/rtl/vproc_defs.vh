// Definiciones compartidas del procesador vectorial (VPROC).
// La especificación completa del ISA está en fpga/ISA.md; si se cambia un código aquí,
// hay que cambiarlo también en fpga/asm/vasm.py.
//
// Formato único de 32 bits:
//   [31:26] opcode | [25:22] rd | [21:18] rs | [17:14] rt | [13:0] imm (con signo)
`ifndef VPROC_DEFS_VH
`define VPROC_DEFS_VH

// ---- Control ----
`define OP_NOP    6'h00
`define OP_HALT   6'h01

// ---- Escalares (registros r0..r15, r0 siempre vale 0) ----
`define OP_ADD    6'h02   // rd = rs + rt
`define OP_SUB    6'h03   // rd = rs - rt
`define OP_MUL    6'h04   // rd = rs * rt (32 bits bajos)
`define OP_ADDI   6'h05   // rd = rs + imm
`define OP_SLLI   6'h06   // rd = rs << imm[4:0]
`define OP_RDCYC  6'h07   // rd = contador de ciclos

// ---- Saltos (relativos al PC de la instrucción, en palabras) ----
`define OP_BNE    6'h08   // si rs != rt: pc = pc + imm
`define OP_BLT    6'h09   // si rs <  rt (con signo): pc = pc + imm
`define OP_JMP    6'h0A   // pc = pc + imm

// ---- Vectoriales (registros v0..v7, LANES carriles de 32 bits) ----
`define OP_VLD    6'h10   // vd[i] = mem[rs + imm + i]           (byte sin signo)
`define OP_VST    6'h11   // mem[rs + imm + i] = sat8(vd[i])     (vd es la fuente)
`define OP_VADD   6'h12   // vd = vs + vt
`define OP_VSUB   6'h13   // vd = vs - vt
`define OP_VMULS  6'h14   // vd = vs * r[rt]
`define OP_VMACS  6'h15   // vd = vd + vs * r[rt]
`define OP_VADDI  6'h16   // vd = vs + imm
`define OP_VSRAI  6'h17   // vd = vs >>> imm[4:0]
`define OP_VABS   6'h18   // vd = |vs|
`define OP_VBCAST 6'h19   // vd[i] = r[rs] en todos los carriles

// ---- Operaciones de la ALU escalar ----
`define ALU_ADD   3'd0
`define ALU_SUB   3'd1
`define ALU_MUL   3'd2
`define ALU_SLL   3'd3
`define ALU_PASSB 3'd4    // resultado = operando B (para RDCYC)

// ---- Operaciones de la unidad vectorial ----
`define VU_ADD    3'd0
`define VU_SUB    3'd1
`define VU_MULS   3'd2
`define VU_MACS   3'd3
`define VU_ADDI   3'd4
`define VU_SRAI   3'd5
`define VU_ABS    3'd6
`define VU_BCAST  3'd7

`endif
