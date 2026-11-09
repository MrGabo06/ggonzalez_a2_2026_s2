# Perfilado: msi-ema

## CPU (perf)

Contadores de todo el proceso (incluye generar la imagen de entrada, igual para todos los
backends), con `--reps/--warmup` iguales dentro de cada tamano. IPC = instrucciones / ciclos.
Fallos de cache = fallos en la cache de ultimo nivel (LLC); L1 = fallos de carga en L1 de datos.

### gauss, 1024x1024

| backend | mediana (ms) | instrucciones (M) | ciclos (M) | IPC | fallos LLC (M) | fallos L1 (M) |
|---|---|---|---|---|---|---|
| seq | 27.937 | 3360.4 | 791.6 | 4.24 | 0.2 | 0.5 |
| seq_sep | 11.025 | 1378.0 | 337.1 | 4.09 | 0.6 | 1.7 |
| simd | 2.983 | 364.0 | 111.5 | 3.26 | 0.2 | 0.4 |
| simd_sep | 1.010 | 225.1 | 72.9 | 3.09 | 0.3 | 1.6 |
| simd512 | 1.901 | 284.1 | 85.3 | 3.33 | 0.1 | 0.4 |
| simd_sep512 | 0.617 | 183.6 | 63.9 | 2.87 | 0.3 | 1.6 |

### gauss, 4096x4096

| backend | mediana (ms) | instrucciones (M) | ciclos (M) | IPC | fallos LLC (M) | fallos L1 (M) |
|---|---|---|---|---|---|---|
| seq | 451.968 | 53714.3 | 12646.5 | 4.25 | 2.4 | 6.7 |
| seq_sep | 200.660 | 22447.0 | 5780.6 | 3.88 | 11.7 | 55.3 |
| simd | 42.133 | 5488.2 | 1612.8 | 3.40 | 2.4 | 5.4 |
| simd_sep | 40.217 | 3944.9 | 1601.5 | 2.46 | 17.5 | 50.8 |
| simd512 | 26.503 | 3925.6 | 1145.1 | 3.43 | 2.4 | 5.3 |
| simd_sep512 | 36.908 | 3225.6 | 1500.8 | 2.15 | 17.9 | 49.7 |

### sobel, 1024x1024

| backend | mediana (ms) | instrucciones (M) | ciclos (M) | IPC | fallos LLC (M) | fallos L1 (M) |
|---|---|---|---|---|---|---|
| seq | 12.457 | 1598.1 | 371.1 | 4.31 | 0.2 | 0.4 |
| seq_sep | 7.666 | 1001.5 | 247.7 | 4.04 | 0.5 | 1.6 |
| simd | 1.353 | 243.2 | 69.0 | 3.52 | 0.1 | 0.4 |
| simd_sep | 0.979 | 201.4 | 68.9 | 2.92 | 0.3 | 1.6 |
| simd512 | 0.978 | 203.6 | 60.1 | 3.39 | 0.1 | 0.4 |
| simd_sep512 | 0.763 | 171.0 | 61.7 | 2.77 | 0.3 | 1.6 |

### sobel, 4096x4096

| backend | mediana (ms) | instrucciones (M) | ciclos (M) | IPC | fallos LLC (M) | fallos L1 (M) |
|---|---|---|---|---|---|---|
| seq | 203.385 | 25514.8 | 5901.2 | 4.32 | 2.4 | 5.7 |
| seq_sep | 148.773 | 16409.1 | 4390.1 | 3.74 | 12.0 | 33.2 |
| simd | 21.072 | 3702.3 | 1025.3 | 3.61 | 2.4 | 5.3 |
| simd_sep | 37.242 | 3560.3 | 1515.5 | 2.35 | 18.1 | 31.6 |
| simd512 | 13.961 | 2933.0 | 847.1 | 3.46 | 2.5 | 5.2 |
| simd_sep512 | 35.366 | 3045.4 | 1460.5 | 2.09 | 19.5 | 30.5 |

## GPU (Nsight Compute)

Un kernel por backend. Nsight Compute fija el reloj de la GPU, asi que `duracion` sirve para
comparar kernels entre si, no como tiempo absoluto (el CSV de `make bench` es el tiempo real).
SM = throughput de computo, L1 = throughput de L1/TEX, DRAM = ancho de banda a memoria, todos en %
del pico. IPC = instrucciones ejecutadas por ciclo activo.

### gauss, 4096x4096

| backend | duracion (ms) | ciclos (M) | instr. (M) | IPC | SM % | L1 % | DRAM % | L1 hit % | ocupacion % | ciclos/instr |
|---|---|---|---|---|---|---|---|---|---|---|
| cuda | 3.370 | 2.75 | 270.0 | 3.27 | 92.6 | 92.7 | 3.4 | 92.9 | 84.9 | 12.42 |
| cuda_tiled | 2.380 | 1.95 | 202.2 | 3.46 | 86.4 | 56.1 | 4.9 | 15.7 | 94.8 | 13.10 |

cuda_tiled / cuda: duracion 0.71x, instrucciones 0.75x.

### sobel, 4096x4096

| backend | duracion (ms) | ciclos (M) | instr. (M) | IPC | SM % | L1 % | DRAM % | L1 hit % | ocupacion % | ciclos/instr |
|---|---|---|---|---|---|---|---|---|---|---|
| cuda | 1.110 | 0.91 | 86.0 | 3.15 | 80.6 | 80.8 | 10.4 | 83.4 | 73.0 | 10.98 |
| cuda_tiled | 1.440 | 1.18 | 116.7 | 3.30 | 82.3 | 43.7 | 8.0 | 15.5 | 92.9 | 13.43 |

cuda_tiled / cuda: duracion 1.30x, instrucciones 1.36x.

