// VPROC: procesador vectorial pequeño para convoluciones de imágenes.
//
// Bloques:
//   PC + memoria de instrucciones -> IR -> decodificador -> unidad de control (multiciclo)
//   banco escalar r0..r15 (32 bits, r0 = 0) + ALU escalar  : direcciones, bucles, saltos
//   banco vectorial v0..v7 (LANES x 32 bits) + unidad vectorial : el cálculo de píxeles
//   memoria de datos entrelazada en LANES bancos            : VLD/VST de LANES bytes
//   contador de ciclos                                      : medición sin sesgo del host
//
// Protocolo con el host (testbench ahora, JTAG en la tarjeta):
//   1. Con el núcleo detenido, escribir el programa (host_imem_*) y la imagen (host_dmem_*).
//   2. Pulso en start: el núcleo arranca en PC = 0 y pone el contador en 0.
//   3. Esperar done = 1 (el programa ejecutó HALT). error = 1 si hubo instrucción ilegal.
//   4. Leer el resultado por host_dmem_* y el número de ciclos en cycles.
`include "vproc_defs.vh"

module vproc_core #(
    parameter LANES      = 8,
    parameter IMEM_WORDS = 512,
    parameter DMEM_BYTES = 16384
) (
    input  wire        clk,
    input  wire        rst,

    // Control
    input  wire        start,
    output wire        busy,
    output wire        done,
    output wire        error,
    output reg  [31:0] cycles,

    // Carga del programa
    input  wire                           host_imem_we,
    input  wire [$clog2(IMEM_WORDS)-1:0]  host_imem_addr,
    input  wire [31:0]                    host_imem_wdata,

    // Acceso a la memoria de datos (byte a byte)
    input  wire        host_dmem_en,
    input  wire        host_dmem_we,
    input  wire [31:0] host_dmem_addr,
    input  wire [7:0]  host_dmem_wdata,
    output wire [7:0]  host_dmem_rdata
);
    localparam PC_W = $clog2(IMEM_WORDS);
    localparam VW   = LANES * 32;

    // ------------------------------------------------------------------ PC e IR
    reg  [PC_W-1:0] pc;
    reg  [31:0]     ir;
    wire [31:0]     imem_q;

    vproc_imem #(.WORDS(IMEM_WORDS)) u_imem (
        .clk(clk), .addr(pc), .q(imem_q),
        .host_we(host_imem_we && !busy), .host_addr(host_imem_addr), .host_wdata(host_imem_wdata)
    );

    // ------------------------------------------------------------------ Decodificador
    wire [5:0]  opcode;
    wire [3:0]  rd, rs, rt;
    wire [31:0] imm;
    wire is_halt, is_branch, is_jump, is_vld, is_vst, illegal;
    wire rf_we, vrf_we, alu_b_imm, alu_b_cyc, br_lt;
    wire [2:0] alu_op, vu_op;

    vproc_decoder u_dec (
        .instr(ir), .opcode(opcode), .rd(rd), .rs(rs), .rt(rt), .imm(imm),
        .is_halt(is_halt), .is_branch(is_branch), .is_jump(is_jump),
        .is_vld(is_vld), .is_vst(is_vst), .illegal(illegal),
        .rf_we(rf_we), .vrf_we(vrf_we),
        .alu_op(alu_op), .alu_b_imm(alu_b_imm), .alu_b_cyc(alu_b_cyc),
        .vu_op(vu_op), .br_lt(br_lt)
    );

    // ------------------------------------------------------------------ Unidad de control
    wire [2:0] state;
    wire ir_load, exec, mem_wb;

    vproc_control u_ctrl (
        .clk(clk), .rst(rst), .start(start),
        .is_halt(is_halt), .is_vld(is_vld), .illegal(illegal),
        .state(state), .busy(busy), .done(done),
        .ir_load(ir_load), .exec(exec), .mem_wb(mem_wb), .error(error)
    );

    // ------------------------------------------------------------------ Bancos de registros
    reg [31:0]   rf  [0:15];
    reg [VW-1:0] vrf [0:7];

    wire [31:0]   r_rs = (rs == 4'd0) ? 32'd0 : rf[rs];
    wire [31:0]   r_rt = (rt == 4'd0) ? 32'd0 : rf[rt];
    wire [VW-1:0] v_rs = vrf[rs[2:0]];
    wire [VW-1:0] v_rt = vrf[rt[2:0]];
    wire [VW-1:0] v_rd = vrf[rd[2:0]];

    // ------------------------------------------------------------------ ALU escalar
    wire [31:0] alu_b = alu_b_cyc ? cycles : (alu_b_imm ? imm : r_rt);
    wire [31:0] alu_y;
    wire        alu_eq, alu_lt;

    vproc_alu u_alu (.op(alu_op), .a(r_rs), .b(alu_b), .y(alu_y), .eq(alu_eq), .lt(alu_lt));

    // ------------------------------------------------------------------ Unidad vectorial
    wire [31:0]   vu_scalar = (vu_op == `VU_BCAST) ? r_rs : r_rt;
    wire [VW-1:0] vu_y;

    vproc_vector_unit #(.LANES(LANES)) u_vu (
        .op(vu_op), .va(v_rs), .vb(v_rt), .vacc(v_rd),
        .scalar(vu_scalar), .imm(imm), .vy(vu_y)
    );

    // ------------------------------------------------------------------ Memoria de datos
    // Dirección efectiva = r[rs] + imm. VST guarda v[rd] saturado a 0..255.
    wire [31:0]        mem_addr = r_rs + imm;
    wire [LANES*8-1:0] st_bytes;
    wire [LANES*8-1:0] ld_bytes;

    genvar i;
    generate
        for (i = 0; i < LANES; i = i + 1) begin : sat
            wire signed [31:0] x = v_rd[i*32 +: 32];
            assign st_bytes[i*8 +: 8] = (x < 0) ? 8'd0 : ((x > 255) ? 8'd255 : x[7:0]);
        end
    endgenerate

    vproc_dmem #(.LANES(LANES), .BYTES(DMEM_BYTES)) u_dmem (
        .clk(clk), .core_sel(busy),
        .core_en(exec && (is_vld || is_vst)), .core_we(exec && is_vst),
        .core_addr(mem_addr), .core_wdata(st_bytes), .core_rdata(ld_bytes),
        .host_en(host_dmem_en), .host_we(host_dmem_we), .host_addr(host_dmem_addr),
        .host_wdata(host_dmem_wdata), .host_rdata(host_dmem_rdata)
    );

    // Bytes leídos -> carriles de 32 bits (sin signo)
    wire [VW-1:0] ld_vec;
    generate
        for (i = 0; i < LANES; i = i + 1) begin : zext
            assign ld_vec[i*32 +: 32] = {24'd0, ld_bytes[i*8 +: 8]};
        end
    endgenerate

    // ------------------------------------------------------------------ Secuencia
    wire take_branch = is_branch && (br_lt ? alu_lt : !alu_eq);
    // Para saltos la ALU compara r[rs] con r[rt] (alu_b = r_rt porque alu_b_imm = 0)

    integer k;
    always @(posedge clk) begin
        if (rst) begin
            pc     <= {PC_W{1'b0}};
            ir     <= 32'd0;
            cycles <= 32'd0;
            for (k = 0; k < 16; k = k + 1) rf[k] <= 32'd0;
        end else begin
            // Arranque: PC y contador a cero
            if (start && !busy) begin
                pc     <= {PC_W{1'b0}};
                cycles <= 32'd0;
            end else if (busy) begin
                cycles <= cycles + 32'd1;
            end

            if (ir_load) ir <= imem_q;

            if (exec) begin
                // Siguiente PC
                if (take_branch || is_jump) pc <= pc + imm[PC_W-1:0];
                else                        pc <= pc + 1'b1;

                // Escrituras
                if (rf_we && rd != 4'd0) rf[rd] <= alu_y;
                if (vrf_we && !is_vld)   vrf[rd[2:0]] <= vu_y;
            end

            if (mem_wb) vrf[rd[2:0]] <= ld_vec;
        end
    end
endmodule
