// Top de la DE1-SoC con acceso por JTAG: la PC carga el programa y la imagen, arranca el
// VPROC y lee el resultado y los ciclos (fpga/jtag/vproc_jtag.tcl, backend fpga de dlp_bench).
//
// A diferencia de fpga/board/de1soc_top.v no trae datos fijos: todo llega por el cable USB
// (USB-Blaster II) a través de un Virtual JTAG (sld_virtual_jtag, instancia 0, IR de 2 bits).
//
// Controles:
//   KEY[0]  reset del procesador y del puente
//   SW[0]   0: displays muestran los ciclos de la última ejecución (decimal, hasta 999 999)
//           1: displays muestran cuántos comandos JTAG llegaron (módulo 256)
// LEDs:
//   LEDR[0] encendido: el diseño está cargado     LEDR[1] procesador ejecutando
//   LEDR[2] terminó (HALT)                        LEDR[7] instrucción ilegal
//   LEDR[9] cambia con cada comando JTAG (parpadea mientras se transfiere)
module de1soc_jtag_top (
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
    localparam IMEM_WORDS = 512;
    localparam DMEM_BYTES = 65536;   // 64 KB en M10K: imágenes grandes se procesan por franjas
    localparam BURST      = 128;     // bytes por scan JTAG (debe coincidir con vproc_jtag.tcl)

    wire clk = CLOCK_50;

    // ------------------------------------------------------------ Reset (KEY[0], activo en bajo)
    reg [2:0] key0_s = 3'b000;
    always @(posedge clk) key0_s <= {key0_s[1:0], KEY[0]};
    wire rst = ~key0_s[2];

    // ------------------------------------------------------------ Virtual JTAG
    wire       tck, tdi, tdo;
    wire [1:0] ir_in;
    wire       cdr, sdr, udr;

    sld_virtual_jtag #(
        .sld_auto_instance_index("YES"),
        .sld_instance_index(0),
        .sld_ir_width(2)
    ) u_vjtag (
        .tck(tck), .tdi(tdi), .tdo(tdo),
        .ir_in(ir_in), .ir_out(2'b00),
        .virtual_state_cdr(cdr), .virtual_state_sdr(sdr), .virtual_state_udr(udr)
    );

    // ------------------------------------------------------------ Puente y procesador
    wire        core_rst, start, busy, done, error;
    wire [31:0] cycles, last_cycles;
    wire        imem_we;
    wire [8:0]  imem_addr;
    wire [31:0] imem_wdata;
    wire        dmem_en, dmem_we;
    wire [31:0] dmem_addr;
    wire [7:0]  dmem_wdata, dmem_rdata;
    wire [7:0]  seq;

    vjtag_bridge #(.BURST(BURST), .IMEM_AW(9)) u_bridge (
        .tck(tck), .tdi(tdi), .tdo(tdo), .ir_in(ir_in), .cdr(cdr), .sdr(sdr), .udr(udr),
        .clk(clk), .rst(rst), .core_rst(core_rst), .start(start),
        .busy(busy), .done(done), .error(error), .cycles(cycles),
        .imem_we(imem_we), .imem_addr(imem_addr), .imem_wdata(imem_wdata),
        .dmem_en(dmem_en), .dmem_we(dmem_we), .dmem_addr(dmem_addr),
        .dmem_wdata(dmem_wdata), .dmem_rdata(dmem_rdata),
        .seq(seq), .last_cycles(last_cycles)
    );

    vproc_core #(.LANES(8), .IMEM_WORDS(IMEM_WORDS), .DMEM_BYTES(DMEM_BYTES)) u_core (
        .clk(clk), .rst(core_rst), .start(start), .busy(busy), .done(done), .error(error),
        .cycles(cycles),
        .host_imem_we(imem_we), .host_imem_addr(imem_addr), .host_imem_wdata(imem_wdata),
        .host_dmem_en(dmem_en), .host_dmem_we(dmem_we), .host_dmem_addr(dmem_addr),
        .host_dmem_wdata(dmem_wdata), .host_dmem_rdata(dmem_rdata)
    );

    // ------------------------------------------------------------ LEDs
    assign LEDR[0]   = 1'b1;
    assign LEDR[1]   = busy;
    assign LEDR[2]   = done;
    assign LEDR[6:3] = 4'd0;
    assign LEDR[7]   = error;
    assign LEDR[8]   = 1'b0;
    assign LEDR[9]   = seq[0];

    // ------------------------------------------------------------ Displays (decimal)
    wire [19:0] shown = SW[0]                           ? {12'd0, seq} :
                        (last_cycles > 32'd999999)      ? 20'd999999   : last_cycles[19:0];
    wire [23:0] bcd;
    bin2bcd20 u_bcd (.bin(shown), .bcd(bcd));

    seg7 d0 (.d(bcd[3:0]),   .seg(HEX0));
    seg7 d1 (.d(bcd[7:4]),   .seg(HEX1));
    seg7 d2 (.d(bcd[11:8]),  .seg(HEX2));
    seg7 d3 (.d(bcd[15:12]), .seg(HEX3));
    seg7 d4 (.d(bcd[19:16]), .seg(HEX4));
    seg7 d5 (.d(bcd[23:20]), .seg(HEX5));
endmodule
