# Comparativa de plataformas: CPU, GPU y FPGA

## FPGA contra la CPU que la maneja (chunchunmaru)

Mismas imágenes y mismos filtros. La CPU se midió en la máquina conectada a la tarjeta, con repeticiones suficientes para los tamaños pequeños.

### gauss: mediana en ms

| | 32² | 64² | 100² | 128² | 256² | 512² |
|---|---|---|---|---|---|---|
| seq | 0.0264 | 0.113 | 0.275 | 0.450 | 1.758 | 7.210 |
| mejor SIMD | 0.0023 (simd_sep) | 0.0058 (simd_sep) | 0.0074 (simd_sep512) | 0.0172 (simd_sep512) | 0.0505 (simd_sep512) | 0.161 (simd_sep512) |
| fpga kernel | 0.609 | 2.404 | 6.075 | 9.558 | 38.420 | 152.6 |
| fpga total (con JTAG) | 8.890 | 32.489 | 83.406 | 128.4 | 520.8 | 2066.1 |
| fpga kernel / total | 6.8 % | 7.4 % | 7.3 % | 7.4 % | 7.4 % | 7.4 % |
| speedup del kernel contra seq | 0.043x | 0.047x | 0.045x | 0.047x | 0.046x | 0.047x |
| speedup del total contra seq | 0.003x | 0.003x | 0.003x | 0.004x | 0.003x | 0.003x |
| speedup del mejor SIMD contra seq | 11.5x | 19.3x | 36.9x | 26.2x | 34.8x | 44.8x |

### sobel: mediana en ms

| | 32² | 64² | 100² | 128² | 256² | 512² |
|---|---|---|---|---|---|---|
| seq | 0.0122 | 0.0493 | 0.122 | 0.199 | 0.795 | 3.179 |
| mejor SIMD | 0.0015 (simd_sep) | 0.0038 (simd_sep) | 0.0056 (simd_sep512) | 0.0113 (simd_sep512) | 0.0336 (simd_sep512) | 0.116 (simd_sep512) |
| fpga kernel | 0.199 | 0.765 | 1.915 | 3.004 | 12.000 | 47.503 |
| fpga total (con JTAG) | 8.741 | 30.692 | 76.599 | 122.4 | 501.3 | 1969.3 |
| fpga kernel / total | 2.3 % | 2.5 % | 2.5 % | 2.5 % | 2.4 % | 2.4 % |
| speedup del kernel contra seq | 0.062x | 0.064x | 0.064x | 0.066x | 0.066x | 0.067x |
| speedup del total contra seq | 0.001x | 0.002x | 0.002x | 0.002x | 0.002x | 0.002x |
| speedup del mejor SIMD contra seq | 8.24x | 13.0x | 22.0x | 17.6x | 23.6x | 27.3x |

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
| chunchunmaru (CPU de la FPGA) | seq | 7.210 | 1.00x | 3.179 | 1.00x |
| chunchunmaru (CPU de la FPGA) | mejor SIMD (simd_sep512) | 0.161 | 44.8x | 0.116 | 27.3x |
| FPGA fpga | kernel (ciclos / 50 MHz) | 152.6 | 0.047x | 47.503 | 0.067x |
| FPGA fpga | total con JTAG | 2066.1 | 0.003x | 1969.3 | 0.002x |
