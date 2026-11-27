# Estrategia de pruebas y resultados

El enunciado pide una estrategia de prueba para validar que el resultado paralelo es igual al secuencial, con la cantidad de pruebas y el tamaño de los datos. Este documento la resume. Los resultados son de la corrida del 2026-11-23 en `chunchunmaru` (i7-1165G7 con AVX-512, sin GPU) con la DE1-SoC conectada, y de la corrida de `test_backends` en una máquina con GPU RTX de un compañero.

## 1. Niveles de prueba

| # | Nivel | Qué comprueba | Comando | Cantidad | Resultado |
|---|---|---|---|---|---|
| 1 | Unitarias | Validador, baseline, estadística y formato `.hex` | `make test` | 20 pruebas | 20 de 20 |
| 2 | Backends contra `seq` | Cada backend da la misma imagen que `seq`, píxel a píxel | `make test_backends SIMD=1 AVX512=1` (CPU) y `make test_backends SIMD=1 CUDA=1` (GPU) | 380 casos en la CPU y 684 con GPU | 380 de 380 y 684 de 684 idénticos |
| 3 | RTL de la FPGA en simulación | El procesador contra la salida de `seq`, y el ISA completo | `make fpga_test` | 6 casos | 6 de 6 PASS |
| 4 | Top de la tarjeta en simulación | Carga, ejecución, comparación y displays | `make fpga_board_sim` | 1 caso | PASS |
| 5 | Camino completo sin tarjeta | C++, Tcl y RTL por el mismo protocolo JTAG, con varias franjas | `make fpga_jtag_sim` | 3 corridas | 3 de 3 `ok` |
| 6 | FPGA en hardware | El backend `fpga` por JTAG contra `seq` | `make fpga_jtag_test` | 4 corridas | 4 de 4 `ok` |
| 7 | Prueba autónoma en la tarjeta | Sobel 64×64 cargado en el `.sof`, 038262 ciclos y `LEDR[9]` | botón `KEY[1]` | 1 caso | PASS (probado por el equipo) |
| 8 | Validación en cada medición | Todas las corridas de los benchmarks usan `--validate` | `make report`, `make fpga_jtag_bench` | 222 corridas en los CSV del repo | 222 con error 0 |
| 9 | Reproducibilidad entre máquinas | Las salidas de CPU, GPU y FPGA son idénticas byte a byte | `make numeric_export`, `make numeric_diff` | 52 digests, 2 máquinas | idénticos en los 3 casos |
| 10 | Modelo y perfil interno | El modelo de ciclos y el SignalTap coinciden con el contador de hardware | `fpga/scripts/pipeline_model.py`, `make fpga_signaltap_report` | 6 casos y 2 capturas | diferencia 0 |
| 11 | Demo con una foto | Una foto real da la misma imagen en todos los backends, comparada byte a byte | `make demo` (con `FPGA=1` incluye la tarjeta) | 14 corridas por foto y tamaño (6 de CPU y la FPGA, 2 filtros), con 2 fotos | `validacion ok` en todas e `IDENTICAS` por filtro, a 512² y 1024² |

**Qué detecta cada nivel.** Los niveles 1 y 2 atrapan errores de lógica y de bordes sin hardware. Los niveles 3 a 5 prueban el RTL y el protocolo antes de tocar la tarjeta. El 6 y el 7 prueban el silicio. El 8 y el 9 garantizan que las mediciones del paper corresponden a salidas correctas. El 11 lo muestra con una foto real y deja las imágenes para verlas.

### Detalle de cada nivel

**1. Unitarias (20).** Imágenes idénticas, un píxel distinto, PSNR conocido, tamaños distintos (debe fallar); Gauss y Sobel sobre imagen constante, determinismo del baseline, kernels separables contra pesos y contra la convolución directa; media, mediana (par e impar, sin modificar la lista, con valor atípico), desviación y lista vacía; ida y vuelta de `.hex`, formato, tamaño equivocado, línea inválida y archivo faltante.

**2. Backends (380).** 5 backends (`seq_sep`, `simd`, `simd_sep`, `simd512`, `simd_sep512`) × 2 filtros × 19 tamaños × 2 semillas. Cada caso corre el backend **dos veces** sobre la misma imagen para detectar estado que sobrevive entre corridas. En una máquina con GPU se agregan `cuda` y `cuda_tiled`, esta última con 5 tamaños de bloque (16×16, 8×8, 32×8, 4×32 y 32×32). En la máquina con la RTX (sin AVX-512) los backends son `seq_sep`, `simd`, `simd_sep`, `cuda` y `cuda_tiled`, es decir 4 + 5 configuraciones × 2 filtros × 19 tamaños × 2 semillas = 684 casos. El backend `fpga` se omite salvo con `DLP_TEST_FPGA=1`, porque va por JTAG y es lento para esta cantidad de casos.

**3. Simulación de la FPGA (6).** `isa` ejercita las 20 instrucciones y compara 64 bytes; `smoke` es la ejecución dummy; Gauss y Sobel a 32×32 y 64×64 (este con semilla 7) se comparan contra la salida de `seq` exportada en `.hex`. Ciclos: 106, 80, 30 425, 9 942, 120 185 y 38 262.

**5. Co-simulación (3).** Gauss 37×37 y Sobel 40×40 con la memoria de datos reducida a 2 KB (`DLP_FPGA_DMEM=2048`), para que se procesen en varias franjas como pasa con las imágenes grandes, y Gauss 32×32 en una franja.

**6. Hardware (4).** Gauss 32×32, Sobel 32×32, Gauss 100×100 (ancho que no es múltiplo de 8, se completa repitiendo la última columna) y Sobel 256×256 (varias franjas).

**8. Validación en las mediciones (222).** 64 corridas de `msi-ema`, 48 de la ASUS con la RTX 3050 Ti, 84 de la línea base de `chunchunmaru` y 26 de la FPGA (barridos de 32² a 1024²). El error máximo es 0 en todas.

**11. Demo con una foto (14 por foto y tamaño).** La foto se copia a `examples/` (git ignora su contenido), se convierte a escala de grises y se recorta al centro a un cuadrado de `DEMO_SIZE` (512 por defecto). Luego corre `seq`, `seq_sep` y las variantes SIMD y CUDA que estén compiladas, cada una con ambos filtros y `--validate`, y con `FPGA=1` el backend `fpga` (después de un `fpga_jtag_ping`). Al final, `make demo_images` escribe un PNG por salida y `montage.png`, y compara los hashes md5 de las salidas de cada filtro; falla si alguna difiere. Todo queda en `examples/output/<foto>/`. Las corridas usan 1 repetición y sin calentamiento, así que sus tiempos no son una medición: las mediciones salen de `make report` y `make fpga_jtag_bench`.

## 2. Datos de prueba

- **Imágenes sintéticas** de `generate_synthetic(ancho, alto, semilla)`: gradiente diagonal, rectángulo central invertido, franjas verticales y ruido uniforme de ±20 con `std::mt19937`, saturado a 0..255. Tienen bordes fuertes para que Sobel detecte algo y ruido para que los errores de redondeo se vean. Son deterministas por semilla.
- **Semillas:** 42 y 7 en `test_backends`; 42 en los benchmarks y los digests; 7 en la simulación de 64×64. La imagen de entrada es idéntica entre máquinas, lo confirma el SHA-256 de cada digest.
- **Tamaños de `test_backends` (19):** 1×1, 1×7, 7×1 (más chicas que el filtro), 2×3, 7×7, 8×8, 9×9, 13×13, 15×15, 16×16, 17×17 (alrededor de los múltiplos de 8 y 16 de los carriles SIMD), 31×33, 37×50, 100×3, 3×100 (rectangulares), 100×100, 257×129, 1023×517 (colas) y 1024×1024.
- **Tamaños de medición:** 512² a 4096² en CPU y GPU, 32² a 1024² en la FPGA (la memoria de datos de 64 KB obliga a franjas desde 256²).
- **Filtros:** Gauss 5×5 y Sobel x 3×3, directos y separables.
- **Reglas de referencia** (`common/filter.h`): el borde replica el píxel más cercano, el resultado es la suma de peso por píxel con valor absoluto, y el redondeo suma 0.5, trunca y satura a 255.

## 3. Criterio de aceptación: tolerancia 0

Todas las comparaciones exigen **diferencia máxima 0**, no una tolerancia de PSNR. Es posible porque CPU y GPU calculan en `float32` pero todos los pesos son múltiplos de 1/256 y toda suma parcial vale menos de 1024 en valor absoluto, lo que necesita como máximo 18 bits significativos de los 24 de la mantisa: no hay redondeo y el orden de la suma no importa. El redondeo `trunc(v + 0.5)` coincide con el `(acc + 128) >> 8` de la FPGA. Cualquier diferencia indica un error real, no ruido numérico.

## 4. Resultados de la corrida del 2026-11-23

| Comando | Salida |
|---|---|
| `make test` | `All tests passed` |
| `make test_backends SIMD=1 AVX512=1` | `Backends probados: seq_sep simd simd_sep simd512 simd_sep512` y `380/380 casos identicos a seq` |
| `make test_backends SIMD=1 CUDA=1` (máquina con GPU) | `Backends probados: seq_sep simd simd_sep cuda cuda_tiled` y `684/684 casos identicos a seq` |
| `make fpga_test` | 6 `PASS`, con `errors=0 max_err=0` en todos |
| `make fpga_board_sim` | `PASS`, `LEDR[9]` encendido |
| `make fpga_jtag_sim` | 3 corridas con `max=0 diff=0 ok=1` |
| `make fpga_jtag_test` | 4 corridas con `max=0 diff=0 ok=1`; ciclos 30 425, 9 942, 303 757 y 600 010 |
| `make demo FPGA=1` (512², corrida del 2026-11-26) | 14 `validacion ok`, hashes idénticos en los 7 backends por filtro; kernel de la FPGA 152.566 ms (Gauss) y 47.503 ms (Sobel) |
| `make demo FPGA=1 DEMO_SIZE=1024` (corrida del 2026-11-26) | Igual, con kernel de la FPGA 620.532 ms (Gauss) y 194.015 ms (Sobel) |

## 5. Cómo reproducirlas

```bash
make test                                  # nivel 1
make test_backends SIMD=1 AVX512=1 -j      # nivel 2 (agregue CUDA=1 en una máquina con GPU)
make fpga_test && make fpga_board_sim      # niveles 3 y 4 (requieren iverilog)
make fpga_jtag_sim                         # nivel 5 (requiere iverilog y tclsh)
make fpga_jtag_test                        # nivel 6 (tarjeta con fpga/jtag/output_files/vproc_jtag.sof)
make numeric_export && make numeric_diff   # nivel 9
```

La demo con una foto (nivel 11), con la foto en `examples/` o indicada con `DEMO_IMAGE=<ruta>`:

```bash
make demo                                  # backends compilados; agregue FPGA=1 con la tarjeta programada
make demo FPGA=1 DEMO_SIZE=1024            # otro lado del recorte (el JTAG tarda unos 21 s por filtro a 1024²)
make demo_seq                              # por familia: demo_seq, demo_simd, demo_cuda y demo_fpga
make demo_images                           # al final: PNG, montage.png y comparación de hashes
```

`demo_cuda` falla con la instrucción de compilación si el binario no se construyó con `CUDA=1`; `make demo` simplemente la omite.

## 6. Límites

- **`test_backends` con GPU se corrió en una sola máquina** (684 de 684). En `chunchunmaru` no se puede, porque no tiene GPU. La segunda máquina con GPU solo se validó con las corridas `--validate` y los digests. Las variantes AVX-512 se probaron únicamente en `chunchunmaru`.
- Las pruebas automáticas y los benchmarks usan **imágenes sintéticas**. Las fotos reales solo se probaron con `make demo` (2 fotos, 512² y 1024², una repetición). La FPGA se probó con imágenes cuadradas, y la demo recorta al cuadrado.
- **`demo_cuda` aún no se ha corrido en una máquina con GPU**: en `chunchunmaru` no hay CUDA.
- Los benchmarks usan **una sola semilla**; la variedad de semillas está en `test_backends`.
- No hay pruebas automáticas de fallos del enlace JTAG (cable desconectado o tiempo agotado): el backend lanza un error, pero no hay un caso que lo verifique.
- **No hay integración continua**: las pruebas se corren a mano. Las que necesitan la tarjeta o una GPU no pueden correr en un servidor genérico.
