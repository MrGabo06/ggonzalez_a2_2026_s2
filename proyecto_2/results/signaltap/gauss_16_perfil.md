```
Perfil de la ejecucion (results/signaltap/gauss_16.csv)

Ventana de la ejecucion
Dato                     Valor
-----------------------  -----
Muestra del pulso start   4097
Muestras con busy = 1     7817
Ciclos start -> done      7818
Termino con done            si
Error en la ejecucion       no

Contador de hardware: 7817 ciclos. Diferencia con las muestras de busy: +0

Ocupacion por estado de la FSM
Estado  Ciclos  % del total
------  ------  -----------
FETCH     2339       29.9 %
DECODE    2339       29.9 %
EXEC      2339       29.9 %
MEM        800       10.2 %

Rendimiento
Dato                                  Valor
------------------------------------  -----
Instrucciones ejecutadas               2339
CPI medido (ciclos / instrucciones)   3.342
VLD ejecutadas                          800
Ciclos MEM (deben ser iguales a VLD)    800
Ciclos si todas fueran de 3 etapas     7017
Ciclos extra por VLD                    800

Mezcla de instrucciones (ciclos = 3 por instruccion + 1 por cada VLD)
Instruccion  Veces  % instr.  Ciclos  % ciclos
-----------  -----  --------  ------  --------
VLD            800    34.2 %    3200    40.9 %
VMACS          800    34.2 %    2400    30.7 %
ADDI           205     8.8 %     615     7.9 %
ADD            192     8.2 %     576     7.4 %
VMULS          160     6.8 %     480     6.1 %
VBCAST          32     1.4 %      96     1.2 %
VADDI           32     1.4 %      96     1.2 %
VSRAI           32     1.4 %      96     1.2 %
VST             32     1.4 %      96     1.2 %
BLT             32     1.4 %      96     1.2 %
BNE             16     0.7 %      48     0.6 %
SLLI             5     0.2 %      15     0.2 %
HALT             1     0.0 %       3     0.0 %

Por clase
Clase      Instr.  % instr.  Ciclos  % ciclos
---------  ------  --------  ------  --------
control         1     0.0 %       3     0.0 %
escalar       402    17.2 %    1206    15.4 %
salto          48     2.1 %     144     1.8 %
memoria       832    35.6 %    3296    42.2 %
vectorial    1056    45.1 %    3168    40.5 %

Bucles: PCs agrupados por numero de ejecuciones
Ejecuciones  Instrucciones     PCs  Ciclos  % ciclos
-----------  -------------  ------  ------  --------
32                      69  21..89    7424    95.0 %
16                       7  18..93     336     4.3 %
1                       19   0..94      57     0.7 %
Bucle interno: 69 instrucciones (25 VLD) x 32 iteraciones = 232 ciclos por iteracion
```
