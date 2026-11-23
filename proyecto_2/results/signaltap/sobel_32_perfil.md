```
Perfil de la ejecucion (results/signaltap/sobel_32.csv)

Ventana de la ejecucion
Dato                     Valor
-----------------------  -----
Muestra del pulso start   4097
Muestras con busy = 1     9942
Ciclos start -> done      9943
Termino con done            si
Error en la ejecucion       no

Contador de hardware: 9942 ciclos. Diferencia con las muestras de busy: +0

Ocupacion por estado de la FSM
Estado  Ciclos  % del total
------  ------  -----------
FETCH     3058       30.8 %
DECODE    3058       30.8 %
EXEC      3058       30.8 %
MEM        768        7.7 %

Rendimiento
Dato                                  Valor
------------------------------------  -----
Instrucciones ejecutadas               3058
CPI medido (ciclos / instrucciones)   3.251
VLD ejecutadas                          768
Ciclos MEM (deben ser iguales a VLD)    768
Ciclos si todas fueran de 3 etapas     9174
Ciclos extra por VLD                    768

Mezcla de instrucciones (ciclos = 3 por instruccion + 1 por cada VLD)
Instruccion  Veces  % instr.  Ciclos  % ciclos
-----------  -----  --------  ------  --------
VLD            768    25.1 %    3072    30.9 %
ADDI           652    21.3 %    1956    19.7 %
VSUB           384    12.6 %    1152    11.6 %
VMACS          384    12.6 %    1152    11.6 %
ADD            320    10.5 %     960     9.7 %
VBCAST         128     4.2 %     384     3.9 %
VABS           128     4.2 %     384     3.9 %
VST            128     4.2 %     384     3.9 %
BLT            128     4.2 %     384     3.9 %
BNE             32     1.0 %      96     1.0 %
SLLI             5     0.2 %      15     0.2 %
HALT             1     0.0 %       3     0.0 %

Por clase
Clase      Instr.  % instr.  Ciclos  % ciclos
---------  ------  --------  ------  --------
control         1     0.0 %       3     0.0 %
escalar       977    31.9 %    2931    29.5 %
salto         160     5.2 %     480     4.8 %
memoria       896    29.3 %    3456    34.8 %
vectorial    1024    33.5 %    3072    30.9 %

Bucles: PCs agrupados por numero de ejecuciones
Ejecuciones  Instrucciones     PCs  Ciclos  % ciclos
-----------  -------------  ------  ------  --------
128                     22  20..41    9216    92.7 %
32                       7  17..45     672     6.8 %
1                       18   0..46      54     0.5 %
Bucle interno: 22 instrucciones (6 VLD) x 128 iteraciones = 72 ciclos por iteracion
```
