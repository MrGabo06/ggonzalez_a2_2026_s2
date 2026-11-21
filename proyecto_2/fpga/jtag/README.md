# VPROC por JTAG en la DE1-SoC

La PC manda el programa y la imagen por el cable USB (USB-Blaster II), el VPROC hace la convolución y la PC lee el resultado y los ciclos. Con esto la FPGA es un backend más de `dlp_bench`:

```bash
./build/dlp_bench --backend fpga --filter gauss --size 256 --validate
```

Usa un Virtual JTAG de Intel, como la guía sugerida en el enunciado ([GuiaJtag](https://github.com/Abner2111/GuiaJtag)). La diferencia es que aquí cada scan transporta un comando completo con 128 bytes de datos.

## Cómo funciona

```
dlp_bench (C++) ──stdin/stdout──> quartus_stp -t vproc_jtag.tcl ──USB/JTAG──> sld_virtual_jtag
                                                                                  │
                                                                    vjtag_bridge.v (comandos)
                                                                                  │
                                                                    vproc_core (puertos host_*)
```

1. `backends/fpga/fpga_backend.cpp` divide la imagen en franjas que caben en los 64 KB de memoria de datos. A cada franja le agrega el borde replicado y completa el ancho a múltiplo de 8. Después ensambla el programa (`fpga/programs/gauss5.s` o `sobel3.s`) para ese tamaño.
2. Arranca una sola vez `quartus_stp -t fpga/jtag/vproc_jtag.tcl server` y le manda comandos de texto: `PROG`, `WRITE`, `RUN`, `READ`.
3. El script convierte cada comando en scans del registro de datos del Virtual JTAG (1064 bits: 40 de encabezado y 128 bytes de datos).
4. `vjtag_bridge.v` recibe el comando en el reloj de TCK y lo pasa al de 50 MHz. Ahí escribe o lee las memorias del VPROC, lo arranca o devuelve los ciclos.
5. El tiempo total de `dlp_bench` incluye las transferencias por JTAG. La línea `kernel` usa los ciclos que contó el hardware (`ciclos / 50 MHz`), igual que con la GPU, y es la que se compara sin sesgo del host.

El formato exacto del registro y de los comandos está al inicio de `vjtag_bridge.v` y de `vproc_jtag.tcl`.

## Archivos

| Archivo | Qué es |
|---|---|
| `de1soc_jtag_top.v` | Top de la tarjeta: Virtual JTAG + puente + VPROC con 64 KB de memoria de datos |
| `vjtag_bridge.v` | Comandos JTAG → puertos `host_*` del VPROC, con el cruce de reloj |
| `vproc_jtag.tcl` | Lado de la PC: `ping` para probar, `server` para el backend |
| `vproc_jtag.qpf` / `.qsf` / `.sdc` | Proyecto Quartus (mismos pines que `fpga/board/`) |
| `sim/` | Modelo del Virtual JTAG y co-simulación con Icarus (sin tarjeta) |

## Pasos

### 1. Probar sin tarjeta (opcional)

Desde `proyecto_2/` (requiere `iverilog` y `tclsh`: `sudo apt install iverilog tcl`):

```bash
make fpga_jtag_sim
```

Corre `dlp_bench --backend fpga` contra la simulación del mismo RTL, por el mismo script Tcl. Las tres corridas deben terminar en `ok=1`. La primera procesa la imagen en dos franjas.

### 2. Compilar y programar

1. Quartus: *File → Open Project* → `fpga/jtag/vproc_jtag.qpf` → *Start Compilation*.
2. *Timing Analyzer*: el *setup slack* de `CLOCK_50` debe ser positivo.
3. *Tools → Programmer* → *Hardware Setup* **DE-SoC** → *Auto Detect* → asignar `output_files/vproc_jtag.sof` al `5CSEMA5` → *Start*.

Al terminar, `LEDR[0]` queda encendido y los displays muestran `000000`.

### 3. Probar el enlace

```bash
make fpga_jtag_ping
```

Debe imprimir `Puente VPROC encontrado en DE-SoC [...]`. Si no encuentra `quartus_stp`, agregue `~/altera_lite/25.1std/quartus/bin` al `PATH` o pase `QUARTUS_STP=/ruta/quartus_stp`.

### 4. Correr el backend

```bash
make fpga_jtag_test                                       # gauss/sobel 32, gauss 100, sobel 256 con --validate
./build/dlp_bench --backend fpga --filter gauss --size 512 --reps 3 --validate --csv results/fpga.csv
```

Mientras transfiere, `LEDR[9]` parpadea. Al terminar, los displays muestran los ciclos de la última franja. Con `SW[0]` arriba muestran cuántos comandos JTAG llegaron.

## Tiempos esperados

- **Cómputo (línea `kernel`):** el mismo número de ciclos que en simulación. Por ejemplo, Gauss 32×32 da 30 425 ciclos (0.61 ms).
- **Total:** lo domina el JTAG. Cada scan es una transacción USB, así que una imagen de 1024×1024 (unas 36 franjas) puede tardar decenas de segundos. Para tamaños grandes conviene `--reps 1 --warmup 0`.
- **Arranque de `quartus_stp`:** tarda unos segundos, pero ocurre en `init()` y no se mide.

## Si algo no funciona

| Mensaje | Qué revisar |
|---|---|
| `no hay cables JTAG` | `jtagconfig` debe listar `DE-SoC`. Revisar las reglas udev y el cable |
| `el puente JTAG no responde` | La tarjeta tiene otro `.sof` (por ejemplo el de `fpga/board/`). Programar `vproc_jtag.sof` |
| `respuesta desfasada` | No debería pasar. Presionar `KEY[0]` y repetir. Si sigue, avisar con el mensaje completo |
| Varias tarjetas o cables | Definir `VPROC_JTAG_HW="DE-SoC [3-1]"` (el nombre que muestra `jtagconfig`) |
