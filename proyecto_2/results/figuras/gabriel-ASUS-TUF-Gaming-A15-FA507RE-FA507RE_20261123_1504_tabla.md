### gauss: mediana en ms (speedup contra seq)

| Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|
| seq | 5.475 (1.0x) | 21.955 (1.0x) | 87.302 (1.0x) | 347.720 (1.0x) |
| seq_sep | 2.214 (2.5x) | 9.099 (2.4x) | 35.683 (2.4x) | 168.383 (2.1x) |
| simd | 0.654 (8.4x) | 2.450 (9.0x) | 9.274 (9.4x) | 36.855 (9.4x) |
| simd_sep | 0.186 (29.4x) | 0.702 (31.3x) | 3.644 (24.0x) | 43.984 (7.9x) |
| cuda | 0.142 (38.7x) | 0.470 (46.7x) | 1.624 (53.7x) | 5.597 (62.1x) |
| cuda_tiled | 0.126 (43.3x) | 0.427 (51.5x) | 1.410 (61.9x) | 4.820 (72.1x) |

### sobel: mediana en ms (speedup contra seq)

| Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|
| seq | 2.529 (1.0x) | 10.124 (1.0x) | 41.318 (1.0x) | 164.067 (1.0x) |
| seq_sep | 1.544 (1.6x) | 5.998 (1.7x) | 24.821 (1.7x) | 128.495 (1.3x) |
| simd | 0.257 (9.8x) | 0.957 (10.6x) | 3.590 (11.5x) | 14.069 (11.7x) |
| simd_sep | 0.135 (18.7x) | 0.616 (16.4x) | 2.921 (14.1x) | 40.919 (4.0x) |
| cuda | 0.112 (22.5x) | 0.401 (25.3x) | 1.253 (33.0x) | 4.299 (38.2x) |
| cuda_tiled | 0.115 (22.0x) | 0.388 (26.1x) | 1.219 (33.9x) | 4.271 (38.4x) |

### Eficiencia de SIMD (speedup contra su secuencial / píxeles por instrucción)

Un valor mayor a 100 % es posible: el baseline secuencial hace más trabajo por píxel que solo multiplicar (clamp en cada vecino, cálculo de índices), y el código SIMD lo evita en el interior de la imagen. Por eso la eficiencia mide el aprovechamiento del vector *más* esa diferencia de código, no solo el paralelismo.

**gauss**

| Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|
| simd (8 por instr.) | 105 % | 112 % | 118 % | 118 % |
| simd_sep (8 por instr.) | 149 % | 162 % | 122 % | 48 % |

**sobel**

| Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|
| simd (8 por instr.) | 123 % | 132 % | 144 % | 146 % |
| simd_sep (8 por instr.) | 143 % | 122 % | 106 % | 39 % |

### Kernel de CUDA sin copias de memoria

El tiempo extremo a extremo incluye copiar la imagen a la GPU y traer el resultado; el del kernel es solo el cálculo en la GPU. 'Copias' es la parte del tiempo total que no es kernel.

**gauss**

| Backend | Dato | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|---|
| cuda | solo kernel | 0.052 ms | 0.163 ms | 0.621 ms | 2.449 ms |
| cuda | extremo a extremo | 0.142 ms | 0.470 ms | 1.624 ms | 5.597 ms |
| cuda | copias | 64 % | 65 % | 62 % | 56 % |
| cuda | speedup del kernel contra seq | 106x | 135x | 141x | 142x |
| cuda_tiled | solo kernel | 0.038 ms | 0.118 ms | 0.449 ms | 1.752 ms |
| cuda_tiled | extremo a extremo | 0.126 ms | 0.427 ms | 1.410 ms | 4.820 ms |
| cuda_tiled | copias | 70 % | 72 % | 68 % | 64 % |
| cuda_tiled | speedup del kernel contra seq | 142x | 186x | 194x | 198x |
| cuda_tiled contra cuda | kernel (mayor a 1 = tiled más rápido) | 1.34x | 1.38x | 1.38x | 1.40x |

**sobel**

| Backend | Dato | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|---|
| cuda | solo kernel | 0.023 ms | 0.056 ms | 0.222 ms | 0.862 ms |
| cuda | extremo a extremo | 0.112 ms | 0.401 ms | 1.253 ms | 4.299 ms |
| cuda | copias | 79 % | 86 % | 82 % | 80 % |
| cuda | speedup del kernel contra seq | 109x | 179x | 186x | 190x |
| cuda_tiled | solo kernel | 0.027 ms | 0.076 ms | 0.282 ms | 1.080 ms |
| cuda_tiled | extremo a extremo | 0.115 ms | 0.388 ms | 1.219 ms | 4.271 ms |
| cuda_tiled | copias | 76 % | 81 % | 77 % | 75 % |
| cuda_tiled | speedup del kernel contra seq | 93x | 134x | 147x | 152x |
| cuda_tiled contra cuda | kernel (mayor a 1 = tiled más rápido) | 0.85x | 0.75x | 0.79x | 0.80x |

### Diferencias numéricas contra seq

Error máximo por píxel en todas las corridas: 0. Backends con alguna corrida fuera de tolerancia: ninguno.
