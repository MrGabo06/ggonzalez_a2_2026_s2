### gauss: mediana en ms (speedup contra seq)

| Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|
| seq | 5.432 (1.0x) | 21.543 (1.0x) | 87.361 (1.0x) | 350.965 (1.0x) |
| seq_sep | 2.213 (2.5x) | 8.735 (2.5x) | 35.297 (2.5x) | 168.638 (2.1x) |
| simd | 0.691 (7.9x) | 2.411 (8.9x) | 9.210 (9.5x) | 36.432 (9.6x) |
| simd_sep | 0.184 (29.5x) | 0.701 (30.7x) | 3.448 (25.3x) | 43.312 (8.1x) |

### sobel: mediana en ms (speedup contra seq)

| Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|
| seq | 2.555 (1.0x) | 10.092 (1.0x) | 40.694 (1.0x) | 161.514 (1.0x) |
| seq_sep | 1.531 (1.7x) | 6.132 (1.6x) | 24.807 (1.6x) | 128.968 (1.3x) |
| simd | 0.265 (9.6x) | 0.950 (10.6x) | 3.653 (11.1x) | 15.161 (10.7x) |
| simd_sep | 0.215 (11.9x) | 0.510 (19.8x) | 2.832 (14.4x) | 41.527 (3.9x) |

### Eficiencia de SIMD (speedup contra su secuencial / píxeles por instrucción)

Un valor mayor a 100 % es posible: el baseline secuencial hace más trabajo por píxel que solo multiplicar (clamp en cada vecino, cálculo de índices), y el código SIMD lo evita en el interior de la imagen. Por eso la eficiencia mide el aprovechamiento del vector *más* esa diferencia de código, no solo el paralelismo.

**gauss**

| Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|
| simd (8 por instr.) | 98 % | 112 % | 119 % | 120 % |
| simd_sep (8 por instr.) | 150 % | 156 % | 128 % | 49 % |

**sobel**

| Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|
| simd (8 por instr.) | 120 % | 133 % | 139 % | 133 % |
| simd_sep (8 por instr.) | 89 % | 150 % | 109 % | 39 % |

### Diferencias numéricas contra seq

Error máximo por píxel en todas las corridas: 0. Backends con alguna corrida fuera de tolerancia: ninguno.
