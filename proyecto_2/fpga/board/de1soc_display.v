// Módulos de los displays de 7 segmentos de la DE1-SoC. Los usan de1soc_top.v (carga fija)
// y fpga/jtag/de1soc_jtag_top.v (JTAG).

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
