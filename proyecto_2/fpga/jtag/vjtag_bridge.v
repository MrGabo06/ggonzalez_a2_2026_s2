// Puente entre el Virtual JTAG de Intel y los puertos host_* del VPROC.
//
// La PC (quartus_stp + fpga/jtag/vproc_jtag.tcl) hace un "scan" de DR de DRW bits por cada
// comando. Con la instrucción IR = 1 (CMD), el registro de datos tiene este formato
// (bit 0 es el primero que entra y el primero que sale):
//
//   Comando (PC -> FPGA)                    Respuesta (FPGA -> PC)
//   [7:0]    op                             [7:0]    0xA5 (para reconocer el puente)
//   [15:8]   cnt (palabras o bytes)         [15:8]   seq: comandos ejecutados (mod 256)
//   [39:16]  addr                           [23:16]  {4'b0, cmd_err, busy, done, error}
//   [40+:8*BURST] datos                     [31:24]  op del comando que respondió
//                                           [39:32]  BURST
//                                           [40+:8*BURST] datos
//
// La respuesta que sale en un scan es la del comando de DOS scans antes. El cable puede pasar
// de Update-DR (llega el comando k) al Capture-DR del scan siguiente en unos pocos TCK, menos
// de lo que tarda en ejecutarse el comando, así que esa captura no puede traer el resultado de k.
// En cambio, el resultado de k sí está listo al final del scan k+1 (son más de 1000 TCK): en el
// Update-DR de k+1 se copia a resp_hold, y el Capture-DR del scan k+2 lo entrega. El script
// tiene esto en cuenta, y usa seq para confirmar que cada respuesta es la que esperaba.
//
// Comandos:
//   0 NOP     no hace nada (sirve para recoger la respuesta del último comando)
//   1 WIMEM   escribe cnt palabras de 32 bits en la memoria de instrucciones desde addr
//   2 WDMEM   escribe cnt bytes en la memoria de datos desde addr
//   3 RDMEM   lee cnt bytes de la memoria de datos desde addr (salen en los datos)
//   4 START   arranca el núcleo en PC = 0
//   5 STATUS  datos[31:0] = ciclos de la última ejecución
//   6 RESET   reinicia el núcleo (por si quedó colgado)
// Escribir o arrancar mientras el núcleo está ocupado no hace nada y marca cmd_err.
//
// Cruce de reloj: el comando se guarda en el dominio de TCK y se avisa al de CLOCK_50 con un
// bit que cambia (toggle) y pasa por 3 flip-flops. Un comando tarda a lo sumo BURST + 4 ciclos
// de 50 MHz (menos de 3 us con BURST = 128) y un scan completo dura más de 40 us con el TCK
// máximo de 24 MHz, así que cmd_q y resp ya están quietos cuando el otro dominio los lee.
// Si alguna vez no fuera así, seq no coincidiría y el script lo reporta.
module vjtag_bridge #(
    parameter BURST   = 128,     // bytes de datos por scan
    parameter IMEM_AW = 9
) (
    // Lado JTAG (sld_virtual_jtag)
    input  wire                tck,
    input  wire                tdi,
    output wire                tdo,
    input  wire [1:0]          ir_in,
    input  wire                cdr,
    input  wire                sdr,
    input  wire                udr,

    // Lado del sistema (CLOCK_50)
    input  wire                clk,
    input  wire                rst,
    output wire                core_rst,
    output reg                 start,
    input  wire                busy,
    input  wire                done,
    input  wire                error,
    input  wire [31:0]         cycles,

    output reg                 imem_we,
    output reg [IMEM_AW-1:0]   imem_addr,
    output reg [31:0]          imem_wdata,

    output reg                 dmem_en,
    output reg                 dmem_we,
    output reg [31:0]          dmem_addr,
    output reg [7:0]           dmem_wdata,
    input  wire [7:0]          dmem_rdata,

    output reg [7:0]           seq,        // para los LEDs: cambia con cada comando
    output reg [31:0]          last_cycles
);
    localparam DRW    = 40 + 8 * BURST;
    localparam IR_CMD = 2'd1;
    localparam [7:0] BURST8 = BURST;

    localparam OP_NOP = 8'd0, OP_WIMEM = 8'd1, OP_WDMEM = 8'd2, OP_RDMEM = 8'd3,
               OP_START = 8'd4, OP_STATUS = 8'd5, OP_RESET = 8'd6;

    // ================================================================== dominio TCK
    reg [DRW-1:0] dr;
    reg [DRW-1:0] cmd_q;
    reg           cmd_tgl = 1'b0;
    reg           bypass  = 1'b0;
    reg [DRW-1:0] resp;          // lo escribe el dominio clk al terminar cada comando
    reg [DRW-1:0] resp_hold = {DRW{1'b0}};  // copia de resp tomada en Update-DR; sale en el Capture-DR

    wire sel = (ir_in == IR_CMD);

    always @(posedge tck) begin
        if (sel) begin
            if (cdr)      dr <= resp_hold;
            else if (sdr) dr <= {tdi, dr[DRW-1:1]};
            if (udr) begin
                cmd_q     <= dr;
                cmd_tgl   <= ~cmd_tgl;
                resp_hold <= resp;   // resultado del comando anterior, ya estable
            end
        end else if (sdr) begin
            bypass <= tdi;
        end
    end

    assign tdo = sel ? dr[0] : bypass;

    // ================================================================== dominio clk
    reg [2:0] tgl_s = 3'b000;
    always @(posedge clk) tgl_s <= {tgl_s[1:0], cmd_tgl};
    wire new_cmd = tgl_s[2] ^ tgl_s[1];

    localparam S_IDLE = 3'd0, S_WIMEM = 3'd1, S_WDMEM = 3'd2, S_RD = 3'd3,
               S_RESET = 3'd5, S_RESP = 3'd6;

    reg [2:0]          st;
    reg [7:0]          op;
    reg [8:0]          cnt;
    reg [8:0]          idx;
    reg [23:0]         addr;
    reg [8*BURST-1:0]  data;
    reg                cmd_err;
    reg [2:0]          rst_cnt;

    assign core_rst = rst || (st == S_RESET);

    always @(posedge clk) begin
        imem_we <= 1'b0;
        dmem_en <= 1'b0;
        dmem_we <= 1'b0;
        start   <= 1'b0;
        if (done) last_cycles <= cycles;

        if (rst) begin
            st          <= S_IDLE;
            seq         <= 8'd0;
            cmd_err     <= 1'b0;
            last_cycles <= 32'd0;
            resp        <= {DRW{1'b0}};
        end else begin
            case (st)
                S_IDLE: if (new_cmd) begin
                    op      <= cmd_q[7:0];
                    cnt     <= (cmd_q[15:8] == 8'd0 || cmd_q[15:8] > BURST) ? BURST : cmd_q[15:8];
                    addr    <= cmd_q[39:16];
                    data    <= cmd_q[DRW-1:40];
                    idx     <= 9'd0;
                    cmd_err <= 1'b0;
                    case (cmd_q[7:0])
                        OP_WIMEM:  if (busy) begin cmd_err <= 1'b1; st <= S_RESP; end else st <= S_WIMEM;
                        OP_WDMEM:  if (busy) begin cmd_err <= 1'b1; st <= S_RESP; end else st <= S_WDMEM;
                        OP_RDMEM:  if (busy) begin cmd_err <= 1'b1; st <= S_RESP; end else st <= S_RD;
                        OP_START: begin
                            if (busy) cmd_err <= 1'b1;
                            else      start   <= 1'b1;
                            st <= S_RESP;
                        end
                        OP_STATUS: begin
                            data <= {{(8*BURST-32){1'b0}}, cycles};
                            st   <= S_RESP;
                        end
                        OP_RESET: begin rst_cnt <= 3'd0; st <= S_RESET; end
                        default:  st <= S_RESP;   // NOP y códigos desconocidos
                    endcase
                end

                // cnt palabras de 32 bits, una por ciclo
                S_WIMEM: begin
                    imem_we    <= 1'b1;
                    imem_addr  <= addr[IMEM_AW-1:0] + idx[IMEM_AW-1:0];
                    imem_wdata <= data[idx*32 +: 32];
                    if (idx + 1'b1 >= cnt || idx + 1'b1 >= BURST / 4) st <= S_RESP;
                    idx <= idx + 1'b1;
                end

                // cnt bytes, uno por ciclo
                S_WDMEM: begin
                    dmem_en    <= 1'b1;
                    dmem_we    <= 1'b1;
                    dmem_addr  <= {8'd0, addr} + idx;
                    dmem_wdata <= data[idx*8 +: 8];
                    if (idx + 1'b1 >= cnt) st <= S_RESP;
                    idx <= idx + 1'b1;
                end

                // Lectura encadenada: la dirección idx sale registrada en este flanco, la memoria
                // la toma en el siguiente y el byte se guarda dos flancos después (idx - 2).
                S_RD: begin
                    if (idx < cnt) begin
                        dmem_en   <= 1'b1;
                        dmem_addr <= {8'd0, addr} + idx;
                    end
                    if (idx >= 9'd2) data[(idx - 9'd2)*8 +: 8] <= dmem_rdata;
                    if (idx == cnt + 9'd1) st <= S_RESP;
                    idx <= idx + 1'b1;
                end

                S_RESET: begin
                    rst_cnt <= rst_cnt + 1'b1;
                    if (rst_cnt == 3'd7) st <= S_RESP;
                end

                S_RESP: begin
                    resp <= {data, BURST8, op, {4'd0, cmd_err, busy, done, error}, seq + 8'd1, 8'hA5};
                    seq  <= seq + 8'd1;
                    st   <= S_IDLE;
                end

                default: st <= S_IDLE;
            endcase
        end
    end
endmodule
