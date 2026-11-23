# Perfilado: gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE

## CPU (perf)

Contadores de todo el proceso (incluye generar la imagen de entrada, igual para todos los
backends), con `--reps/--warmup` iguales dentro de cada tamano. IPC = instrucciones / ciclos.
Fallos de cache = fallos en la cache de ultimo nivel (LLC); L1 = fallos de carga en L1 de datos.

### gauss, 1024x1024

| backend | mediana (ms) | instrucciones (M) | ciclos (M) | IPC | fallos LLC (M) | fallos L1 (M) |
|---|---|---|---|---|---|---|
| seq | 33.033 | 3361.6 | 619.9 | 5.42 | 0.2 | 0.8 |
| seq_sep | 8.787 | 1380.4 | 282.1 | 4.89 | 0.3 | 2.3 |
| simd | 3.264 | 364.6 | 124.6 | 2.93 | 0.2 | 0.9 |
| simd_sep | 0.819 | 227.2 | 69.3 | 3.28 | 0.2 | 2.2 |

### gauss, 4096x4096

| backend | mediana (ms) | instrucciones (M) | ciclos (M) | IPC | fallos LLC (M) | fallos L1 (M) |
|---|---|---|---|---|---|---|
| seq | 368.184 | 53727.1 | 10013.2 | 5.37 | 1.9 | 19.8 |
| seq_sep | 183.293 | 22495.5 | 5209.0 | 4.32 | 5.6 | 65.7 |
| simd | 46.683 | 5496.8 | 1676.8 | 3.28 | 1.2 | 12.5 |
| simd_sep | 53.232 | 3993.8 | 1833.4 | 2.18 | 4.3 | 66.2 |

### sobel, 1024x1024

| backend | mediana (ms) | instrucciones (M) | ciclos (M) | IPC | fallos LLC (M) | fallos L1 (M) |
|---|---|---|---|---|---|---|
| seq | 10.767 | 1598.4 | 319.8 | 5.00 | 0.2 | 0.7 |
| seq_sep | 6.569 | 1003.9 | 215.4 | 4.66 | 0.3 | 2.2 |
| simd | 1.380 | 243.7 | 66.0 | 3.69 | 0.2 | 0.7 |
| simd_sep | 1.106 | 204.0 | 71.6 | 2.85 | 0.2 | 2.2 |

### sobel, 4096x4096

| backend | mediana (ms) | instrucciones (M) | ciclos (M) | IPC | fallos LLC (M) | fallos L1 (M) |
|---|---|---|---|---|---|---|
| seq | 169.142 | 25523.1 | 5030.0 | 5.07 | 1.4 | 15.2 |
| seq_sep | 145.785 | 16451.2 | 4262.2 | 3.86 | 4.7 | 53.1 |
| simd | 22.221 | 3709.9 | 991.2 | 3.74 | 1.0 | 9.4 |
| simd_sep | 50.836 | 3602.0 | 1742.9 | 2.07 | 3.9 | 52.1 |

## GPU (Nsight Compute)

Un kernel por backend. Nsight Compute fija el reloj de la GPU, asi que `duracion` sirve para
comparar kernels entre si, no como tiempo absoluto (el CSV de `make bench` es el tiempo real).
SM = throughput de computo, L1 = throughput de L1/TEX, DRAM = ancho de banda a memoria, todos en %
del pico. IPC = instrucciones ejecutadas por ciclo activo.

### gauss, 4096x4096

| backend | duracion (ms) | ciclos (M) | instr. (M) | IPC | SM % | L1 % | DRAM % | L1 hit % | ocupacion % | ciclos/instr |
|---|---|---|---|---|---|---|---|---|---|---|
| cuda | 2.830 | 4.14 | 270.0 | 3.26 | 92.5 | 92.5 | 6.2 | 93.0 | 81.5 | 11.95 |
| cuda_tiled | 2.010 | 2.95 | 202.2 | 3.44 | 85.8 | 55.7 | 8.7 | 17.2 | 95.1 | 13.24 |

cuda_tiled / cuda: duracion 0.71x, instrucciones 0.75x.

### sobel, 4096x4096

| backend | duracion (ms) | ciclos (M) | instr. (M) | IPC | SM % | L1 % | DRAM % | L1 hit % | ocupacion % | ciclos/instr |
|---|---|---|---|---|---|---|---|---|---|---|
| cuda | 0.970 | 1.42 | 86.0 | 3.04 | 77.7 | 77.8 | 18.1 | 84.9 | 71.4 | 11.16 |
| cuda_tiled | 1.230 | 1.80 | 116.7 | 3.25 | 81.2 | 43.1 | 14.3 | 16.2 | 93.3 | 13.68 |

cuda_tiled / cuda: duracion 1.27x, instrucciones 1.36x.

