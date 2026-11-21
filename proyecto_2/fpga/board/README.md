# VPROC en la DE1-SoC

Primera prueba en hardware, sin JTAG: la tarjeta trae el programa, la imagen y la salida esperada en sus memorias desde que se programa. Al presionar un botón ejecuta el filtro, compara contra `seq` y muestra el resultado en los LEDs y los ciclos en los displays.

## Archivos

| Archivo | Qué es |
|---|---|
| `de1soc_top.v` | Top de la tarjeta. Carga las memorias, arranca el VPROC, compara la salida y maneja los LEDs y displays |
| `de1soc_display.v` | Conversión a decimal y displays de 7 segmentos (también lo usa `fpga/jtag/`) |
| `tb_de1soc_top.v` | Simulación del top completo, incluyendo la pulsación de los botones |
| `vproc_de1soc.qpf` / `.qsf` | Proyecto Quartus: dispositivo 5CSEMA5F31C6, archivos y pines |
| `de1soc.sdc` | Restricción del reloj de 50 MHz |
| `data/` | `prog.hex`, `in_pad.hex`, `exp.hex` y `params.vh`. Los genera `make fpga_board` |

## Pasos

Desde `proyecto_2/`:

```bash
make fpga_board FILTER=gauss SIZE=32    # genera data/ (también acepta sobel y SIZE hasta 64)
make fpga_board_sim                     # simula el top: debe terminar en PASS con los mismos ciclos
```

En Quartus Prime (Lite 18.1 o posterior, con soporte para Cyclone V):

1. *File → Open Project* → `fpga/board/vproc_de1soc.qpf`.
2. *Processing → Start Compilation*.
3. Revisar el reporte de *Timing Analyzer*: el *setup slack* de `CLOCK_50` tiene que ser positivo.
4. Conectar la tarjeta por el USB-Blaster (el puerto USB junto a la entrada de energía) y encenderla.
5. *Tools → Programmer* → *Hardware Setup* → *DE-SoC*. Después *Auto Detect*, elegir `5CSEMA5` y asignarle `output_files/vproc_de1soc.sof`, marcar *Program/Configure* y *Start*.

Cada vez que se cambie `data/` hay que volver a compilar, porque los datos van dentro del `.sof`.

## En la tarjeta

| Control | Función |
|---|---|
| `KEY[0]` | Reset: vuelve a cargar el programa y la imagen |
| `KEY[1]` | Ejecutar (se puede repetir) |
| `SW[0]` | 0: los displays muestran los ciclos. 1: muestran cuántos píxeles difieren de `seq` |

| LED | Significado |
|---|---|
| `LEDR[0]` | Programa e imagen cargados |
| `LEDR[1]` | Procesador ejecutando (dura menos de 3 ms, casi no se ve) |
| `LEDR[2]` | Terminó |
| `LEDR[7]` | Instrucción ilegal |
| `LEDR[8]` | FAIL: hay diferencias |
| `LEDR[9]` | PASS: salida idéntica a `seq` |

Resultado esperado con Gauss 32×32: `LEDR[9]` encendido y `030425` en los displays, los mismos ciclos que en simulación. A 50 MHz son 0.61 ms.

## Si algo no funciona

- **No enciende ningún LED:** revisar que el `.sof` se haya cargado y que los pines del `.qsf` coincidan con los del System Builder de Terasic o con el `DE1_SoC_Golden_Top`.
- **`LEDR[0]` no enciende:** la carga inicial no terminó. Presionar `KEY[0]`.
- **`LEDR[8]` (FAIL):** poner `SW[0]` en 1 para ver cuántos píxeles difieren y comparar contra `make fpga_board_sim`.
- **Slack negativo:** bajar el reloj con un PLL a 25 MHz. Los ciclos no cambian; solo cambia el tiempo.

## Siguiente paso

La versión con JTAG, donde la PC manda cualquier imagen y lee el resultado (backend `fpga` de `dlp_bench`), está en `fpga/jtag/`.
