# Comparativa de plataformas: CPU, GPU y FPGA

## FPGA contra la CPU que la maneja (chunchunmaru)

Mismas imágenes y mismos filtros. La CPU se midió en la máquina conectada a la tarjeta, con repeticiones suficientes para los tamaños pequeños.

### gauss: mediana en ms

| | 32² | 64² | 100² | 128² | 256² | 512² |
|---|---|---|---|---|---|---|
| seq | 0.0269 | 0.110 | 0.263 | 0.440 | 1.831 | 7.108 |
| mejor SIMD | 0.0023 (simd_sep) | 0.0066 (simd_sep512) | 0.0074 (simd_sep512) | 0.0164 (simd_sep512) | 0.0566 (simd_sep512) | 0.194 (simd_sep512) |
| fpga kernel | 0.609 | 2.404 | 6.075 | 9.558 | 38.420 | 152.6 |
| fpga total (con JTAG) | 9.620 | 33.498 | 83.976 | 134.6 | 550.5 | 2147.9 |
| fpga kernel / total | 6.3 % | 7.2 % | 7.2 % | 7.1 % | 7.0 % | 7.1 % |
| speedup del kernel contra seq | 0.044x | 0.046x | 0.043x | 0.046x | 0.048x | 0.047x |
| speedup del total contra seq | 0.003x | 0.003x | 0.003x | 0.003x | 0.003x | 0.003x |
| speedup del mejor SIMD contra seq | 11.9x | 16.7x | 35.5x | 26.9x | 32.3x | 36.6x |

### sobel: mediana en ms

| | 32² | 64² | 100² | 128² | 256² | 512² |
|---|---|---|---|---|---|---|
| seq | 0.0119 | 0.0482 | 0.119 | 0.194 | 0.779 | 3.085 |
| mejor SIMD | 0.0016 (simd_sep) | 0.0043 (simd_sep) | 0.0056 (simd_sep512) | 0.0114 (simd_sep512) | 0.0363 (simd_sep512) | 0.129 (simd_sep512) |
| fpga kernel | 0.199 | 0.765 | 1.915 | 3.004 | 12.000 | 47.503 |
| fpga total (con JTAG) | 8.359 | 28.934 | 76.083 | 121.9 | 492.1 | 2036.4 |
| fpga kernel / total | 2.4 % | 2.6 % | 2.5 % | 2.5 % | 2.4 % | 2.3 % |
| speedup del kernel contra seq | 0.060x | 0.063x | 0.062x | 0.065x | 0.065x | 0.065x |
| speedup del total contra seq | 0.001x | 0.002x | 0.002x | 0.002x | 0.002x | 0.002x |
| speedup del mejor SIMD contra seq | 7.61x | 11.1x | 21.3x | 17.1x | 21.5x | 23.8x |

## Todas las plataformas en 512² (el tamaño común)

El speedup de cada fila es contra el `seq` de **su propia máquina**. Las columnas de CUDA con copias incluyen el viaje de la imagen a la GPU y de vuelta; el kernel no.

| Plataforma | Dato | gauss (ms) | gauss speedup | sobel (ms) | sobel speedup |
|---|---|---|---|---|---|
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | seq | 5.475 | 1.00x | 2.529 | 1.00x |
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | mejor SIMD (simd_sep) | 0.186 | 29.4x | 0.135 | 18.7x |
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | mejor CUDA con copias (cuda_tiled) | 0.126 | 43.3x | 0.112 | 22.5x |
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | mejor CUDA solo kernel (cuda_tiled) | 0.0384 | 142.5x | 0.0232 | 108.9x |
| msi-ema | seq | 7.361 | 1.00x | 3.202 | 1.00x |
| msi-ema | mejor SIMD (simd_sep512) | 0.170 | 43.4x | 0.141 | 22.7x |
| msi-ema | mejor CUDA con copias (cuda_tiled) | 0.172 | 42.9x | 0.170 | 18.9x |
| msi-ema | mejor CUDA solo kernel (cuda_tiled) | 0.0246 | 298.8x | 0.0166 | 192.8x |
| chunchunmaru (CPU de la FPGA) | seq | 7.108 | 1.00x | 3.085 | 1.00x |
| chunchunmaru (CPU de la FPGA) | mejor SIMD (simd_sep512) | 0.194 | 36.6x | 0.129 | 23.8x |
| FPGA fpga | kernel (ciclos / 50 MHz) | 152.6 | 0.047x | 47.503 | 0.065x |
| FPGA fpga | total con JTAG | 2147.9 | 0.003x | 2036.4 | 0.002x |
