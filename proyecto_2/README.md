# Framework experimental de modelos de ejecución DLP (Proyecto 2)

Convolución 2D de imágenes en escala de grises (1 byte por píxel) con dos filtros, Gaussiano 5×5 y Sobel x 3×3, ejecutada con distintos modelos de paralelismo a nivel de datos y comparada con métricas cuantitativas. Todos los backends reciben la misma imagen y el mismo filtro, y se validan contra el baseline secuencial con tolerancia 0.

| Backend | Qué es |
|---|---|
| `seq`, `seq_sep` | Baseline secuencial: convolución directa y separable |
| `simd`, `simd_sep` | Intrinsics AVX2: directa y separable |
| `simd512`, `simd_sep512` | Las mismas con AVX-512 |
| `cuda`, `cuda_tiled` | GPU: un hilo por píxel en memoria global, y tiles en memoria compartida con halo |
| `fpga` | Procesador vectorial propio (VPROC, 20 instrucciones) en la DE1-SoC, por JTAG |

## Estructura

| Carpeta | Contenido |
|---|---|
| `src/` | `main.cpp`: la CLI `dlp_bench` |
| `common/` | Imágenes PGM y sintéticas, filtros, validador, estadística y formato `.hex` |
| `backends/` | Un archivo por backend: `seq/`, `simd/`, `cuda/`, `fpga/` y el registro |
| `tests/` | Pruebas unitarias y de todos los backends contra `seq` |
| `scripts/` | Barridos, tablas, gráficas, perfilado y la demo con una foto |
| `examples/` | Fotos de prueba para `make demo` (git ignora su contenido) y sus salidas en `output/` |
| `fpga/` | ISA, RTL, ensamblador, programas, simulación, y los proyectos Quartus de `board/` y `jtag/` |
| `results/` | CSV de las mediciones, figuras, perfilados, digests y reportes de Quartus |

## Requisitos

- `g++` con C++17 y `python3` con `matplotlib`.
- Opcionales: `nvcc` (backends CUDA), `perf` y Nsight Compute (perfilado), `iverilog`, `tclsh` y `gtkwave` (simulación de la FPGA), y Quartus Prime Lite 25.1 con la DE1-SoC (hardware).

## Inicio rápido

```bash
make build SIMD=1 -j                  # agregue AVX512=1 y CUDA=1 si la máquina los soporta
./build/dlp_bench --backend simd --filter gauss --size 1024 --reps 5 --validate
make help                             # todos los comandos y opciones
```

Al cambiar `SIMD`, `AVX512`, `CUDA` o `DEBUG` hay que ejecutar `make clean` antes. Los comandos `make report`, `make profile`, `make numeric_export` y `make fpga_baseline` detectan solos lo que soporta la máquina.

## Comandos por tarea

| Tarea | Comando |
|---|---|
| Pruebas unitarias y de backends | `make test`, `make test_backends SIMD=1 AVX512=1 CUDA=1 -j` |
| Filtrar una foto con todos los backends y comparar las salidas | Copie la foto a `examples/` y ejecute `make demo` (agregue `FPGA=1` con la tarjeta programada) |
| Lo mismo, por familia de backends | `make demo_prepare`, `make demo_seq`, `make demo_simd`, `make demo_cuda`, `make demo_fpga` y, al final, `make demo_images` |
| Medir CPU y GPU y generar tablas y gráficas | `make report` |
| Perfilado con `perf` y Nsight Compute | `make profile` |
| Eficiencia paralela, ancho de banda | `make efficiency`, `make bandwidth` |
| Diferencias numéricas entre CPU, GPU y FPGA | `make numeric_export` en cada máquina, y `make numeric_diff` |
| Simular la FPGA sin tarjeta | `make fpga_test`, `make fpga_board_sim`, `make fpga_jtag_sim` |
| Probar la FPGA en la tarjeta (JTAG) | `make fpga_jtag_ping`, `make fpga_jtag_test` |
| Medir la FPGA y compararla con CPU y GPU | `make fpga_jtag_bench`, `make fpga_baseline`, `make compare` |
| Perfil interno con SignalTap | `make fpga_signaltap_report STP_CSV=<archivo> STP_CYCLES=<ciclos>` |
| Recursos y Fmax de Quartus | `make fpga_resources` (después de compilar los proyectos en Quartus) |

## Dónde están los resultados

| Ruta | Qué hay |
|---|---|
| `results/<máquina>_*.csv`, `results/figuras/<máquina>_*` | Barrido de CPU y GPU por máquina: tiempos, speedup, eficiencia de SIMD |
| `results/perfilado/` | `perf` (CPU) y Nsight Compute (GPU) |
| `results/fpga/` | Barridos de la FPGA y la línea base de la CPU que la maneja |
| `results/figuras/comparativa_*` | La FPGA junto a CPU y GPU |
| `results/figuras/eficiencia_paralela.*`, `ancho_de_banda.*` | Eficiencia paralela de CUDA y FPGA, y ancho de banda de memoria |
| `results/numeric/` | Digests por máquina y `diferencias.md` |
| `results/signaltap/` | Perfil interno del VPROC (Sobel 32×32 y Gauss 16×16) |
| `results/quartus/` | Recursos y Fmax de los diseños |
| `examples/output/<foto>/` | Salidas de `make demo` (no se suben a git): `montage.png`, `png/` y un `.pgm` por filtro y backend |

## Documentos

- `MILESTONES.md`: arquitectura del framework y estado de los milestones.
- `fpga/ISA.md`: el ISA del VPROC, su microarquitectura y resultados.
- `fpga/PROPUESTAS.md`: las dos propuestas de microarquitectura y su comparación.
- `fpga/board/README.md` y `fpga/jtag/README.md`: cómo compilar, programar y medir en la tarjeta.
- `PRUEBAS.md`: estrategia de pruebas, datos y resultados.

## Notas de uso

- **Tarjeta:** el cable aparece como `DE-SoC [3-1]` en `jtagconfig` y la FPGA es el dispositivo `@2` de la cadena. Para el backend `fpga` y cualquier tiempo hay que programar `fpga/jtag/output_files/vproc_jtag.sof`. El `.sof` con SignalTap infla los tiempos totales porque comparte el cable.
- **Reportes de Quartus:** `output_files/`, `db/` y similares no se suben al repo. `make fpga_resources` solo funciona donde se compiló.
- **Medición:** las corridas de las tablas usan repeticiones y calentamiento. Los tiempos de CPU varían entre corridas por la frecuencia del portátil. El tiempo de kernel de la FPGA sale del contador de ciclos y es determinista.
- **Git:** una rama por tarea desde `develop`, commits convencionales en inglés y PR hacia `develop` con merge commit.
