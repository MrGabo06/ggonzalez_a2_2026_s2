// Modelo de simulación del Virtual JTAG de Intel (solo para Icarus; Quartus usa el IP real).
// Tiene los mismos puertos que usa de1soc_jtag_top.v. Las señales las mueven las tareas
// set_ir y dr_scan, que llama el testbench como lo haría el hub JTAG al recibir un scan:
//   Capture-DR (1 TCK) -> Shift-DR (n TCK, bit 0 primero) -> Update-DR (1 TCK).
`timescale 1ns / 1ps

module sld_virtual_jtag #(
    parameter sld_auto_instance_index = "YES",
    parameter sld_instance_index      = 0,
    parameter sld_ir_width            = 2
) (
    output reg                     tck = 1'b0,
    output reg                     tdi = 1'b0,
    input  wire                    tdo,
    output reg  [sld_ir_width-1:0] ir_in = {sld_ir_width{1'b0}},
    input  wire [sld_ir_width-1:0] ir_out,
    output reg                     virtual_state_cdr = 1'b0,
    output reg                     virtual_state_sdr = 1'b0,
    output reg                     virtual_state_udr = 1'b0
);
    localparam MAXB = 4096;

    task tick;   // TCK de 10 MHz
        begin
            #50 tck = 1'b1;
            #50 tck = 1'b0;
        end
    endtask

    task set_ir(input [sld_ir_width-1:0] v);
        begin
            ir_in = v;
            tick;
        end
    endtask

    task dr_scan(input integer n, input [MAXB-1:0] din, output [MAXB-1:0] dout);
        integer k;
        begin
            dout = {MAXB{1'b0}};
            virtual_state_cdr = 1'b1; tick; virtual_state_cdr = 1'b0;
            virtual_state_sdr = 1'b1;
            for (k = 0; k < n; k = k + 1) begin
                tdi = din[k];
                #1 dout[k] = tdo;          // TDO se lee antes del flanco que desplaza
                tick;
            end
            virtual_state_sdr = 1'b0;
            virtual_state_udr = 1'b1; tick; virtual_state_udr = 1'b0;
            tick;
        end
    endtask
endmodule
