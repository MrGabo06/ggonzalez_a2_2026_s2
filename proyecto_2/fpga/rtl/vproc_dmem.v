// Memoria de datos en chip, entrelazada en LANES bancos de bytes.
//
// El byte con dirección A vive en el banco (A mod LANES), fila (A / LANES). Así un VLD o
// VST de LANES bytes consecutivos toca cada banco exactamente una vez, aunque la dirección
// no esté alineada: el carril i usa el banco (A + i) mod LANES. Esto permite leer en un
// solo ciclo las ventanas desplazadas 1, 2, 3... píxeles que necesita la convolución,
// sin line buffers ni reacomodos.
//
// Dos puertos que se multiplexan (nunca se usan a la vez):
//   - núcleo: LANES bytes por acceso, mientras el programa corre (core_sel = 1)
//   - host:   un byte por acceso, para cargar la imagen y leer el resultado (JTAG/testbench)
// Las lecturas son síncronas (1 ciclo de latencia), como un bloque de RAM de la FPGA.
module vproc_dmem #(
    parameter LANES  = 8,
    parameter BYTES  = 16384,             // capacidad total en bytes
    parameter ADDR_W = 32
) (
    input  wire                 clk,
    input  wire                 core_sel,

    // Puerto del núcleo (vectorial)
    input  wire                 core_en,
    input  wire                 core_we,
    input  wire [ADDR_W-1:0]    core_addr,
    input  wire [LANES*8-1:0]   core_wdata,  // byte del carril i en [i*8 +: 8]
    output wire [LANES*8-1:0]   core_rdata,

    // Puerto del host (byte)
    input  wire                 host_en,
    input  wire                 host_we,
    input  wire [ADDR_W-1:0]    host_addr,
    input  wire [7:0]           host_wdata,
    output wire [7:0]           host_rdata
);
    localparam LG    = $clog2(LANES);
    localparam DEPTH = BYTES / LANES;
    localparam ROW_W = $clog2(DEPTH);

    // Desplazamiento de la dirección dentro de una fila de bancos y fila base
    wire [LG-1:0]    core_off  = core_addr[LG-1:0];
    wire [ROW_W-1:0] core_row  = core_addr[LG +: ROW_W];
    wire [LG-1:0]    host_bank = host_addr[LG-1:0];
    wire [ROW_W-1:0] host_row  = host_addr[LG +: ROW_W];

    // Se registra qué banco corresponde a cada carril para alinear la salida
    reg [LG-1:0] core_off_q;
    reg [LG-1:0] host_bank_q;
    always @(posedge clk) begin
        core_off_q  <= core_off;
        host_bank_q <= host_bank;
    end

    wire [7:0] bank_q [0:LANES-1];

    genvar b;
    generate
        for (b = 0; b < LANES; b = b + 1) begin : bank
            localparam [LG-1:0] BI = b;

            // Carril del núcleo que cae en este banco y fila que le toca:
            // si b < off, el carril ya pasó a la siguiente fila de bancos.
            wire [LG-1:0]    lane     = BI - core_off;
            wire [ROW_W-1:0] c_row    = core_row + ((BI < core_off) ? 1'b1 : 1'b0);
            wire [7:0]       c_wdata  = core_wdata[lane*8 +: 8];

            // Multiplexor de puerto: núcleo o host
            wire             en    = core_sel ? core_en : (host_en && host_bank == BI);
            wire             we    = core_sel ? core_we : host_we;
            wire [ROW_W-1:0] row   = core_sel ? c_row   : host_row;
            wire [7:0]       wdata = core_sel ? c_wdata : host_wdata;

            wire [7:0]       q;

            vproc_bram #(.WIDTH(8), .DEPTH(DEPTH)) u_ram (
                .clk(clk), .en(en), .we(we), .addr(row), .wdata(wdata), .q(q)
            );
            assign bank_q[b] = q;
        end
    endgenerate

    // Salida del núcleo: el carril i viene del banco (off + i) mod LANES
    genvar i;
    generate
        for (i = 0; i < LANES; i = i + 1) begin : rd_lane
            localparam [LG-1:0] II = i;
            wire [LG-1:0] src = core_off_q + II;
            assign core_rdata[i*8 +: 8] = bank_q[src];
        end
    endgenerate

    assign host_rdata = bank_q[host_bank_q];
endmodule
