// Simulación del top de la DE1-SoC: suelta el reset, presiona KEY[1] y espera PASS o FAIL.
// Se ejecuta desde fpga/board/ (make fpga_board_sim), porque el top lee data/*.hex.
`timescale 1ns / 1ps

module tb_de1soc_top;
    reg        clk = 1'b0;
    reg  [3:0] key = 4'b1110;   // KEY[0] presionado = reset
    reg  [9:0] sw  = 10'd0;
    wire [9:0] ledr;
    wire [6:0] hex0, hex1, hex2, hex3, hex4, hex5;
    integer t;

    always #10 clk = ~clk;   // 50 MHz

    de1soc_top dut (
        .CLOCK_50(clk), .KEY(key), .SW(sw), .LEDR(ledr),
        .HEX0(hex0), .HEX1(hex1), .HEX2(hex2), .HEX3(hex3), .HEX4(hex4), .HEX5(hex5)
    );

    // Valor que muestra un display (inverso de seg7)
    function [3:0] digit(input [6:0] s);
        case (s)
            7'b1000000: digit = 0; 7'b1111001: digit = 1; 7'b0100100: digit = 2;
            7'b0110000: digit = 3; 7'b0011001: digit = 4; 7'b0010010: digit = 5;
            7'b0000010: digit = 6; 7'b1111000: digit = 7; 7'b0000000: digit = 8;
            7'b0010000: digit = 9; default:    digit = 15;
        endcase
    endfunction

    task show(input [8*20-1:0] label);
        $display("%0s: %0d%0d%0d%0d%0d%0d", label, digit(hex5), digit(hex4), digit(hex3),
                 digit(hex2), digit(hex1), digit(hex0));
    endtask

    task press_run;
        begin
            key[1] = 1'b0; repeat (5) @(posedge clk);
            key[1] = 1'b1; repeat (5) @(posedge clk);
        end
    endtask

    task wait_finished;
        begin
            t = 0;
            while (!ledr[2] && t < 20_000_000) begin @(posedge clk); t = t + 1; end
        end
    endtask

    initial begin
        repeat (10) @(posedge clk);
        key[0] = 1'b1;                                   // suelta el reset

        t = 0;
        while (!ledr[0] && t < 1_000_000) begin @(posedge clk); t = t + 1; end
        $display("Carga terminada: LEDR[0]=%b", ledr[0]);

        press_run;
        wait_finished;
        $display("LEDR = %b (9 PASS, 8 FAIL, 7 ilegal, 2 terminó, 0 cargado)", ledr);
        show("Ciclos (SW0=0)");
        sw[0] = 1'b1; #1;
        show("Diferencias (SW0=1)");
        sw[0] = 1'b0;

        press_run;                                       // segunda corrida: debe dar lo mismo
        wait_finished;
        show("Ciclos, 2da corrida");

        if (ledr[9] && !ledr[8]) $display("PASS");
        else                     $display("FAIL");
        $finish;
    end
endmodule
