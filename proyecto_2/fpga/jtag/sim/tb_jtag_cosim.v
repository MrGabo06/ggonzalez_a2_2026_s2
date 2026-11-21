// Co-simulación del top JTAG: el mismo script vproc_jtag.tcl que se usa con la tarjeta manda
// sus scans por un FIFO (+req) y este testbench los aplica al modelo del Virtual JTAG y
// devuelve lo capturado por otro FIFO (+resp). Lo arranca fpga/jtag/sim/cosim.py.
//   "IR <valor>"            -> responde "OK"
//   "DR <bits> <hex>"       -> responde el valor capturado en hex
//   "Q" o fin del archivo   -> termina
`timescale 1ns / 1ps

module tb_jtag_cosim;
    reg clk = 1'b0;
    always #10 clk = ~clk;   // 50 MHz

    reg  [3:0] key = 4'b1111;
    reg  [9:0] sw  = 10'd0;
    wire [9:0] ledr;
    wire [6:0] hex0, hex1, hex2, hex3, hex4, hex5;

    de1soc_jtag_top dut (
        .CLOCK_50(clk), .KEY(key), .SW(sw), .LEDR(ledr),
        .HEX0(hex0), .HEX1(hex1), .HEX2(hex2), .HEX3(hex3), .HEX4(hex4), .HEX5(hex5)
    );

    reg [8*512-1:0] f_req, f_resp;
    reg [8*8-1:0]   cmd;
    reg [4095:0]    din, dout;
    integer fr, fw, r, n, v, scans;

    initial begin
        if (!$value$plusargs("req=%s", f_req) || !$value$plusargs("resp=%s", f_resp)) begin
            $display("ERROR: faltan +req y +resp");
            $finish;
        end
        repeat (10) @(posedge clk);
        fr = $fopen(f_req, "r");
        fw = $fopen(f_resp, "w");
        scans = 0;
        while (1) begin
            r = $fscanf(fr, "%s", cmd);
            if (r != 1 || cmd == "Q") begin
                $display("cosim: %0d scans, %0d us simulados", scans, $time / 1000);
                $finish;
            end
            if (cmd == "IR") begin
                r = $fscanf(fr, "%d", v);
                dut.u_vjtag.set_ir(v);
                $fwrite(fw, "OK\n");
            end else begin
                r = $fscanf(fr, "%d %h", n, din);
                dut.u_vjtag.dr_scan(n, din, dout);
                $fwrite(fw, "%0h\n", dout);
                scans = scans + 1;
                // Sin pausa entre scans: el cable puede encadenar Update-DR y el siguiente
                // Capture-DR en pocos TCK, y el protocolo tiene que funcionar igual.
            end
            $fflush(fw);
        end
    end
endmodule
