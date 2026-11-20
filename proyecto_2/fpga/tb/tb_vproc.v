// Testbench del VPROC. Hace el papel del host (lo que después hará el PC por JTAG):
//   1. carga el programa ensamblado en la memoria de instrucciones
//   2. carga la imagen de entrada (.hex del framework) con el borde replicado de R píxeles
//   3. arranca el núcleo y espera HALT
//   4. lee la salida, la compara con la salida del baseline (.hex) y reporta ciclos
//
// Parámetros por plusargs (los pone fpga/scripts/run_sim.py):
//   +prog=programa.hex +in=entrada.hex +exp=esperado.hex [+out=salida.hex]
//   +W=ancho +H=alto +R=radio_del_filtro +IN=dir_entrada +OUT=dir_salida [+vcd=ondas.vcd]
`timescale 1ns / 1ps

module tb_vproc;
    localparam LANES      = 8;
    localparam IMEM_WORDS = 512;
    localparam DMEM_BYTES = 65536;
    localparam MAX_PIX    = 65536;
    localparam TIMEOUT    = 200_000_000;

    reg clk = 1'b0;
    reg rst = 1'b1;
    always #10 clk = ~clk;   // 50 MHz, como el reloj típico de las tarjetas de Intel

    reg         start = 1'b0;
    wire        busy, done, error;
    wire [31:0] cycles;

    reg         imem_we = 1'b0;
    reg  [8:0]  imem_addr = 9'd0;
    reg  [31:0] imem_wdata = 32'd0;

    reg         dmem_en = 1'b0;
    reg         dmem_we = 1'b0;
    reg  [31:0] dmem_addr = 32'd0;
    reg  [7:0]  dmem_wdata = 8'd0;
    wire [7:0]  dmem_rdata;

    vproc_core #(.LANES(LANES), .IMEM_WORDS(IMEM_WORDS), .DMEM_BYTES(DMEM_BYTES)) dut (
        .clk(clk), .rst(rst), .start(start), .busy(busy), .done(done), .error(error),
        .cycles(cycles),
        .host_imem_we(imem_we), .host_imem_addr(imem_addr), .host_imem_wdata(imem_wdata),
        .host_dmem_en(dmem_en), .host_dmem_we(dmem_we), .host_dmem_addr(dmem_addr),
        .host_dmem_wdata(dmem_wdata), .host_dmem_rdata(dmem_rdata)
    );

    // Copias de los registros vectoriales para verlos en gtkwave ($dumpvars no guarda arreglos)
    wire [LANES*32-1:0] dbg_v0 = dut.vrf[0];
    wire [LANES*32-1:0] dbg_v1 = dut.vrf[1];
    wire [LANES*32-1:0] dbg_v2 = dut.vrf[2];
    wire [31:0]         dbg_r1 = dut.rf[1];
    wire [31:0]         dbg_r3 = dut.rf[3];

    reg [31:0] prog [0:IMEM_WORDS-1];
    reg [7:0]  img  [0:MAX_PIX-1];
    reg [7:0]  exp_img [0:MAX_PIX-1];
    reg [7:0]  got  [0:MAX_PIX-1];

    reg [8*512-1:0] f_prog, f_in, f_exp, f_out, f_vcd;   // rutas de hasta 512 caracteres
    integer W, H, R, IN_BASE, OUT_BASE, WP, HP;
    integer x, y, sx, sy, k, errors, max_err, diff, fd, t;

    // Escrituras y lecturas del host: se cambian las señales en el flanco de bajada
    // Termina con error si el archivo no existe (si no, $readmemh deja todo en 'x')
    task must_exist(input [8*512-1:0] path);
        integer f;
        begin
            f = $fopen(path, "r");
            if (f == 0) begin
                $display("ERROR: no se puede abrir %0s", path);
                $display("FAIL");
                $finish;
            end
            $fclose(f);
        end
    endtask

    task host_write(input [31:0] addr, input [7:0] data);
        begin
            @(negedge clk);
            dmem_en = 1'b1; dmem_we = 1'b1; dmem_addr = addr; dmem_wdata = data;
            @(negedge clk);
            dmem_en = 1'b0; dmem_we = 1'b0;
        end
    endtask

    task host_read(input [31:0] addr, output [7:0] data);
        begin
            @(negedge clk);
            dmem_en = 1'b1; dmem_we = 1'b0; dmem_addr = addr;
            @(negedge clk);
            dmem_en = 1'b0;
            data = dmem_rdata;
        end
    endtask

    initial begin
        if (!$value$plusargs("prog=%s", f_prog) || !$value$plusargs("in=%s", f_in) ||
            !$value$plusargs("exp=%s", f_exp) || !$value$plusargs("W=%d", W) ||
            !$value$plusargs("H=%d", H) || !$value$plusargs("R=%d", R) ||
            !$value$plusargs("IN=%d", IN_BASE) || !$value$plusargs("OUT=%d", OUT_BASE)) begin
            $display("ERROR: faltan plusargs (+prog +in +exp +W +H +R +IN +OUT)");
            $finish;
        end
        if ($value$plusargs("vcd=%s", f_vcd)) begin
            $dumpfile(f_vcd);
            $dumpvars(0, tb_vproc);
        end
        WP = W + 2 * R;
        HP = H + 2 * R;

        must_exist(f_prog);
        must_exist(f_in);
        must_exist(f_exp);
        for (k = 0; k < IMEM_WORDS; k = k + 1) prog[k] = 32'h0000_0000;  // NOP
        $readmemh(f_prog, prog);
        $readmemh(f_in, img, 0, W * H - 1);
        $readmemh(f_exp, exp_img, 0, W * H - 1);

        // Reset
        repeat (4) @(posedge clk);
        @(negedge clk) rst = 1'b0;

        // 1. Programa
        for (k = 0; k < IMEM_WORDS; k = k + 1) begin
            @(negedge clk);
            imem_we = 1'b1; imem_addr = k; imem_wdata = prog[k];
        end
        @(negedge clk) imem_we = 1'b0;

        // 2. Imagen con borde replicado (el host hace el padding, el núcleo no revisa bordes)
        for (y = 0; y < HP; y = y + 1)
            for (x = 0; x < WP; x = x + 1) begin
                sy = y - R; if (sy < 0) sy = 0; if (sy > H - 1) sy = H - 1;
                sx = x - R; if (sx < 0) sx = 0; if (sx > W - 1) sx = W - 1;
                host_write(IN_BASE + y * WP + x, img[sy * W + sx]);
            end

        // 3. Ejecutar
        @(negedge clk) start = 1'b1;
        @(negedge clk) start = 1'b0;
        t = 0;
        while (!done && t < TIMEOUT) begin
            @(posedge clk);
            t = t + 1;
        end
        if (!done) begin
            $display("ERROR: timeout, el programa no llegó a HALT");
            $finish;
        end
        if (error) $display("ERROR: instrucción ilegal en PC = %0d", dut.pc);

        // 4. Leer y comparar
        errors = 0;
        max_err = 0;
        for (k = 0; k < W * H; k = k + 1) begin
            host_read(OUT_BASE + k, got[k]);
            if ((^got[k] === 1'bx) || (^exp_img[k] === 1'bx)) begin
                diff = 256;  // dato sin definir: cuenta como error
            end else begin
                diff = got[k] - exp_img[k];
            end
            if (diff < 0) diff = -diff;
            if (diff > max_err) max_err = diff;
            if (diff != 0) begin
                if (errors < 10)
                    $display("  diferencia en (%0d, %0d): fpga=%0d esperado=%0d",
                             k % W, k / W, got[k], exp_img[k]);
                errors = errors + 1;
            end
        end

        if ($value$plusargs("out=%s", f_out)) begin
            fd = $fopen(f_out, "w");
            for (k = 0; k < W * H; k = k + 1) $fwrite(fd, "%02X\n", got[k]);
            $fclose(fd);
        end

        $display("RESULT pixels=%0d cycles=%0d cycles_per_pixel=%0.2f errors=%0d max_err=%0d",
                 W * H, cycles, cycles * 1.0 / (W * H), errors, max_err);
        if (errors == 0 && !error) $display("PASS");
        else                       $display("FAIL");
        $finish;
    end
endmodule
