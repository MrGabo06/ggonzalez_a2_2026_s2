// Unidad de control multiciclo del VPROC.
//
// Cada instrucción pasa por estas etapas (una por ciclo):
//   FETCH  : se presenta el PC a la memoria de instrucciones (lectura síncrona)
//   DECODE : se guarda la instrucción en el IR
//   EXEC   : se leen registros, opera la ALU escalar o la unidad vectorial, se decide el
//            siguiente PC y se escribe el resultado. VST escribe la memoria en este ciclo.
//   MEM    : solo VLD; los bancos entregan los datos y se escriben en v[rd]
// CPI: 3 para casi todo, 4 para VLD.
//
// IDLE espera la orden de inicio del host. HALTED indica que el programa terminó y el
// host puede leer resultados y el contador de ciclos.
module vproc_control (
    input  wire clk,
    input  wire rst,
    input  wire start,          // pulso del host: arrancar desde PC = 0
    input  wire is_halt,
    input  wire is_vld,
    input  wire illegal,

    output reg  [2:0] state,
    output wire busy,           // el núcleo es dueño de las memorias
    output wire done,           // el programa terminó (HALT o instrucción ilegal)
    output wire ir_load,        // DECODE: cargar el IR
    output wire exec,           // EXEC: actualizar PC y escribir resultados
    output wire mem_wb,         // MEM: escribir el dato de VLD
    output reg  error           // se ejecutó una instrucción ilegal
);
    localparam S_IDLE   = 3'd0;
    localparam S_FETCH  = 3'd1;
    localparam S_DECODE = 3'd2;
    localparam S_EXEC   = 3'd3;
    localparam S_MEM    = 3'd4;
    localparam S_HALTED = 3'd5;

    assign busy    = (state != S_IDLE) && (state != S_HALTED);
    assign done    = (state == S_HALTED);
    assign ir_load = (state == S_DECODE);
    assign exec    = (state == S_EXEC) && !is_halt && !illegal;
    assign mem_wb  = (state == S_MEM);

    always @(posedge clk) begin
        if (rst) begin
            state <= S_IDLE;
            error <= 1'b0;
        end else begin
            case (state)
                S_IDLE, S_HALTED:
                    if (start) begin
                        state <= S_FETCH;
                        error <= 1'b0;
                    end
                S_FETCH:  state <= S_DECODE;
                S_DECODE: state <= S_EXEC;
                S_EXEC: begin
                    if (is_halt || illegal) begin
                        state <= S_HALTED;
                        error <= illegal;
                    end else if (is_vld) begin
                        state <= S_MEM;
                    end else begin
                        state <= S_FETCH;
                    end
                end
                S_MEM:    state <= S_FETCH;
                default:  state <= S_IDLE;
            endcase
        end
    end
endmodule
