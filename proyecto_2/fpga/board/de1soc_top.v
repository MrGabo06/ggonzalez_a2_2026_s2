// Top para la DE1-SoC (Cyclone V 5CSEMA5F31C6): corre el VPROC en la tarjeta sin JTAG.
//
// Hace en hardware lo mismo que el testbench (fpga/tb/tb_vproc.v):
//   1. Al salir de reset copia el programa (data/prog.hex) a la memoria de instrucciones y la
//      imagen con borde (data/in_pad.hex) a la memoria de datos, por los puertos host_*.
//   2. Al presionar KEY[1] arranca el procesador y espera HALT.
//   3. Lee la salida y la compara byte a byte con la de seq (data/exp.hex).
//   4. Muestra el resultado en LEDs y los ciclos (o las diferencias) en los displays.
// Los archivos de data/ los genera: python3 fpga/scripts/run_sim.py --filter gauss --size 32 --board
//
// Controles:
//   KEY[0]  reset (vuelve a cargar todo)
//   KEY[1]  ejecutar (se puede repetir; la entrada no se modifica)
//   SW[0]   0: displays muestran los ciclos de la última ejecución (decimal)
//           1: displays muestran cuántos píxeles no coinciden con seq
// LEDs:
//   LEDR[0] programa e imagen cargados        LEDR[1] procesador ejecutando
//   LEDR[2] terminó (HALT)                    LEDR[7] instrucción ilegal
//   LEDR[8] FAIL: alguna diferencia           LEDR[9] PASS: salida idéntica a seq
module de1soc_top (
    input  wire       CLOCK_50,
    input  wire [3:0] KEY,
    input  wire [9:0] SW,
    output wire [9:0] LEDR,
    output wire [6:0] HEX0,
    output wire [6:0] HEX1,
    output wire [6:0] HEX2,
    output wire [6:0] HEX3,
    output wire [6:0] HEX4,
    output wire [6:0] HEX5
);
    `include "params.vh"   // IN_BASE, IN_BYTES, OUT_BASE, OUT_BYTES (de data/)

    localparam IMEM_WORDS = 512;
    localparam DMEM_BYTES = 16384;

    wire clk = CLOCK_50;

    // ------------------------------------------------------------ Botones (activos en bajo)
    reg [2:0] key0_s = 3'b111;
    reg [2:0] key1_s = 3'b111;
    always @(posedge clk) begin
        key0_s <= {key0_s[1:0], KEY[0]};
        key1_s <= {key1_s[1:0], KEY[1]};
    end
    wire rst      = ~key0_s[2];
    wire run_req  = key1_s[2] & ~key1_s[1];   // flanco de bajada = se presionó

    // ------------------------------------------------------------ Memorias de inicialización
    reg [31:0] prog_rom [0:IMEM_WORDS-1];
    reg [7:0]  in_rom   [0:IN_BYTES-1];
    reg [7:0]  exp_rom  [0:OUT_BYTES-1];
    initial begin
        $readmemh("data/prog.hex", prog_rom);
        $readmemh("data/in_pad.hex", in_rom);
        $readmemh("data/exp.hex", exp_rom);
    end

    reg  [15:0] idx;
    reg  [31:0] prog_q;
    reg  [7:0]  in_q, exp_q;
    always @(posedge clk) begin   // lectura síncrona: se infieren bloques de RAM
        prog_q <= prog_rom[idx[8:0]];
        in_q   <= in_rom[idx];
        exp_q  <= exp_rom[idx];
    end

    // ------------------------------------------------------------ Procesador
    reg         start;
    wire        busy, done, error;
    wire [31:0] cycles;
    reg         imem_we;
    reg         dmem_en, dmem_we;
    reg  [31:0] dmem_addr;
    wire [7:0]  dmem_rdata;

    vproc_core #(.LANES(8), .IMEM_WORDS(IMEM_WORDS), .DMEM_BYTES(DMEM_BYTES)) u_core (
        .clk(clk), .rst(rst), .start(start), .busy(busy), .done(done), .error(error),
        .cycles(cycles),
        .host_imem_we(imem_we), .host_imem_addr(idx[8:0]), .host_imem_wdata(prog_q),
        .host_dmem_en(dmem_en), .host_dmem_we(dmem_we), .host_dmem_addr(dmem_addr),
        .host_dmem_wdata(in_q), .host_dmem_rdata(dmem_rdata)
    );

    // ------------------------------------------------------------ Secuenciador (el "host")
    // Cada dato usa dos ciclos: en *_A se presenta la dirección a la ROM o a la memoria,
    // en *_W el dato ya está disponible y se escribe o se compara.
    localparam S_PROG_A = 4'd0, S_PROG_W = 4'd1, S_IMG_A = 4'd2, S_IMG_W = 4'd3,
               S_READY  = 4'd4, S_START  = 4'd5, S_RUN   = 4'd6,
               S_CHK_A  = 4'd7, S_CHK_W  = 4'd8, S_DONE  = 4'd9;

    reg [3:0]  st;
    reg [19:0] mismatches;
    reg [31:0] last_cycles;
    reg        loaded, finished;

    always @(*) begin
        imem_we   = (st == S_PROG_W);
        dmem_en   = (st == S_IMG_W) || (st == S_CHK_A) || (st == S_CHK_W);
        dmem_we   = (st == S_IMG_W);
        dmem_addr = (st == S_IMG_W) ? IN_BASE + idx : OUT_BASE + idx;
        start     = (st == S_START);
    end

    always @(posedge clk) begin
        if (rst) begin
            st         <= S_PROG_A;
            idx        <= 16'd0;
            mismatches <= 20'd0;
            last_cycles<= 32'd0;
            loaded     <= 1'b0;
            finished   <= 1'b0;
        end else begin
            case (st)
                S_PROG_A: st <= S_PROG_W;
                S_PROG_W: if (idx == IMEM_WORDS - 1) begin idx <= 0; st <= S_IMG_A; end
                          else begin idx <= idx + 1'b1; st <= S_PROG_A; end
                S_IMG_A:  st <= S_IMG_W;
                S_IMG_W:  if (idx == IN_BYTES - 1) begin idx <= 0; loaded <= 1'b1; st <= S_READY; end
                          else begin idx <= idx + 1'b1; st <= S_IMG_A; end
                S_READY, S_DONE:
                          if (run_req) begin
                              idx        <= 0;
                              mismatches <= 0;
                              finished   <= 1'b0;
                              st         <= S_START;
                          end
                S_START:  st <= S_RUN;
                S_RUN:    if (done) begin last_cycles <= cycles; st <= S_CHK_A; end
                S_CHK_A:  st <= S_CHK_W;
                S_CHK_W: begin
                    if (dmem_rdata != exp_q) mismatches <= mismatches + 1'b1;
                    if (idx == OUT_BYTES - 1) begin finished <= 1'b1; st <= S_DONE; end
                    else begin idx <= idx + 1'b1; st <= S_CHK_A; end
                end
                default:  st <= S_PROG_A;
            endcase
        end
    end

    // ------------------------------------------------------------ LEDs
    assign LEDR[0]   = loaded;
    assign LEDR[1]   = busy;
    assign LEDR[2]   = finished;
    assign LEDR[6:3] = 4'd0;
    assign LEDR[7]   = error;
    assign LEDR[8]   = finished && (mismatches != 0 || error);
    assign LEDR[9]   = finished && (mismatches == 0 && !error);

    // ------------------------------------------------------------ Displays (decimal)
    wire [19:0] shown = SW[0] ? mismatches : last_cycles[19:0];
    wire [23:0] bcd;
    bin2bcd20 u_bcd (.bin(shown), .bcd(bcd));

    seg7 d0 (.d(bcd[3:0]),   .seg(HEX0));
    seg7 d1 (.d(bcd[7:4]),   .seg(HEX1));
    seg7 d2 (.d(bcd[11:8]),  .seg(HEX2));
    seg7 d3 (.d(bcd[15:12]), .seg(HEX3));
    seg7 d4 (.d(bcd[19:16]), .seg(HEX4));
    seg7 d5 (.d(bcd[23:20]), .seg(HEX5));
endmodule

// Binario de 20 bits a 6 dígitos BCD (double dabble). Valores mayores a 999 999 se recortan.
module bin2bcd20 (
    input  wire [19:0] bin,
    output reg  [23:0] bcd
);
    integer i, j;
    reg [43:0] s;
    always @(*) begin
        s = {24'd0, (bin > 20'd999999) ? 20'd999999 : bin};
        for (i = 0; i < 20; i = i + 1) begin
            for (j = 0; j < 6; j = j + 1)
                if (s[20 + 4*j +: 4] >= 4'd5) s[20 + 4*j +: 4] = s[20 + 4*j +: 4] + 4'd3;
            s = s << 1;
        end
        bcd = s[43:20];
    end
endmodule

// Display de 7 segmentos de la DE1-SoC: activo en bajo, seg[0] = a ... seg[6] = g.
module seg7 (
    input  wire [3:0] d,
    output reg  [6:0] seg
);
    always @(*) begin
        case (d)
            4'd0: seg = 7'b1000000;
            4'd1: seg = 7'b1111001;
            4'd2: seg = 7'b0100100;
            4'd3: seg = 7'b0110000;
            4'd4: seg = 7'b0011001;
            4'd5: seg = 7'b0010010;
            4'd6: seg = 7'b0000010;
            4'd7: seg = 7'b1111000;
            4'd8: seg = 7'b0000000;
            4'd9: seg = 7'b0010000;
            default: seg = 7'b1111111;
        endcase
    end
endmodule
