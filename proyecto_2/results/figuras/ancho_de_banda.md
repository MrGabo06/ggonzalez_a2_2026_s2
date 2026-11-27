# Ancho de banda de memoria

Todo sale de lo ya medido. No hay contadores de DRAM en la CPU ni en la FPGA, así que fuera de la GPU (Nsight) los valores son un mínimo derivado, y se indica en cada tabla.

## CPU: ancho de banda efectivo (GB/s)

T / mediana, con T = 2 x N² bytes. Es un mínimo: no cuenta los accesos que resuelve la caché ni el relleno de bordes. En los tamaños que caben en caché no es tráfico de DRAM.

### gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE

| Filtro | Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|---|
| gauss | seq | 0.10 | 0.10 | 0.10 | 0.10 |
| gauss | mejor SIMD | 2.82 | 2.99 | 2.30 | 0.91 |
| sobel | seq | 0.21 | 0.21 | 0.20 | 0.20 |
| sobel | mejor SIMD | 3.89 | 3.41 | 2.87 | 2.38 |

### msi-ema

| Filtro | Backend | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|---|
| gauss | seq | 0.07 | 0.07 | 0.07 | 0.08 |
| gauss | mejor SIMD | 3.09 | 3.25 | 2.58 | 1.61 |
| sobel | seq | 0.16 | 0.17 | 0.17 | 0.18 |
| sobel | mejor SIMD | 3.72 | 3.80 | 3.23 | 3.47 |

### chunchunmaru (línea base de la FPGA)

| Filtro | Backend | 32² | 64² | 100² | 128² | 256² | 512² | 1024² |
|---|---|---|---|---|---|---|---|---|
| gauss | seq | 0.08 | 0.07 | 0.08 | 0.07 | 0.07 | 0.07 | 0.07 |
| gauss | mejor SIMD | 0.91 | 1.24 | 2.70 | 2.00 | 2.31 | 2.70 | 3.01 |
| sobel | seq | 0.17 | 0.17 | 0.17 | 0.17 | 0.17 | 0.17 | 0.17 |
| sobel | mejor SIMD | 1.31 | 1.88 | 3.59 | 2.88 | 3.62 | 4.05 | 4.01 |

## GPU: kernel, copias y DRAM

- **Kernel:** T / tiempo del kernel (cudaEvent).
- **Copias (PCIe):** T / (extremo a extremo − kernel). Mueve N² bytes de ida y N² de vuelta.
- **DRAM (Nsight):** el ancho de banda que mide Nsight Compute en la memoria del dispositivo, a 4096². El pico se deriva de Nsight como GB/s / (% de DRAM).

### gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE

| Backend | Filtro | Dato | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|---|---|
| cuda | gauss | kernel | 10.2 | 12.9 | 13.5 | 13.7 |
| cuda | gauss | copias (PCIe) | 5.8 | 6.8 | 8.4 | 10.7 |
| cuda | sobel | kernel | 22.6 | 37.2 | 37.8 | 38.9 |
| cuda | sobel | copias (PCIe) | 5.9 | 6.1 | 8.1 | 9.8 |
| cuda_tiled | gauss | kernel | 13.6 | 17.7 | 18.7 | 19.2 |
| cuda_tiled | gauss | copias (PCIe) | 6.0 | 6.8 | 8.7 | 10.9 |
| cuda_tiled | sobel | kernel | 19.2 | 27.7 | 29.8 | 31.1 |
| cuda_tiled | sobel | copias (PCIe) | 6.0 | 6.7 | 8.9 | 10.5 |

### msi-ema

| Backend | Filtro | Dato | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|---|---|
| cuda | gauss | kernel | 12.4 | 16.6 | 17.3 | 17.7 |
| cuda | gauss | copias (PCIe) | 3.5 | 3.8 | 4.2 | 4.8 |
| cuda | sobel | kernel | 31.6 | 50.0 | 55.3 | 58.9 |
| cuda | sobel | copias (PCIe) | 3.4 | 3.8 | 4.5 | 4.6 |
| cuda_tiled | gauss | kernel | 21.3 | 27.2 | 28.3 | 29.3 |
| cuda_tiled | gauss | copias (PCIe) | 3.6 | 3.8 | 3.6 | 4.7 |
| cuda_tiled | sobel | kernel | 24.8 | 35.6 | 41.8 | 43.9 |
| cuda_tiled | sobel | copias (PCIe) | 3.3 | 3.7 | 4.3 | 4.7 |

### DRAM medida por Nsight Compute a 4096²

| Máquina | Backend | Filtro | DRAM (GB/s) | Movido | Movido / mínimo | Pico (GB/s) | % del pico | L1 hit | L2 hit |
|---|---|---|---|---|---|---|---|---|---|
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | cuda | gauss | 11.8 | 33.4 MB | 1.00x | 192 | 6.2 % | 93 % | 85 % |
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | cuda | sobel | 34.6 | 33.6 MB | 1.00x | 192 | 18.1 % | 85 % | 82 % |
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | cuda_tiled | gauss | 16.6 | 33.4 MB | 1.00x | 192 | 8.7 % | 17 % | 85 % |
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | cuda_tiled | sobel | 27.4 | 33.7 MB | 1.00x | 192 | 14.3 % | 16 % | 84 % |
| msi-ema | cuda | gauss | 9.9 | 33.3 MB | 0.99x | 288 | 3.4 % | 93 % | 86 % |
| msi-ema | cuda | sobel | 30.0 | 33.3 MB | 0.99x | 288 | 10.4 % | 83 % | 84 % |
| msi-ema | cuda_tiled | gauss | 14.0 | 33.4 MB | 0.99x | 288 | 4.9 % | 16 % | 86 % |
| msi-ema | cuda_tiled | sobel | 23.1 | 33.2 MB | 0.99x | 288 | 8.0 % | 16 % | 84 % |

## FPGA: memoria interna y enlace JTAG

- **Memoria de datos del VPROC:** 8 bancos de un byte, pico de 8 bytes por ciclo (400 MB/s a 50 MHz). Cada `VLD` y `VST` mueve 8 bytes. Los bytes por píxel salen de la secuencia de instrucciones del modelo (validado contra los ciclos medidos) y el tiempo, del contador de hardware.
- **Enlace JTAG:** bytes que baja y sube el backend por imagen / (total − kernel). Incluye la conversión a texto y el Tcl del lado de la PC. El máximo en bruto del cable es de unos 3 MB/s (24 MHz de TCK).

### gauss

Por píxel de salida: 25 B leídos y 1 B escritos en la memoria interna, contra 1 B de entrada y 1 B de salida mínimos: la entrada se lee 25 veces (sin reutilización de la ventana).

|  | 32² | 64² | 100² | 128² | 256² | 512² | 1024² |
|---|---|---|---|---|---|---|---|
| Kernel, T / tiempo (MB/s) | 3.4 | 3.4 | 3.3 | 3.4 | 3.4 | 3.4 | 3.4 |
| Memoria interna (MB/s) | 43.8 | 44.3 | 42.8 | 44.6 | 44.4 | 44.7 | 43.9 |
| Uso del pico de la memoria interna | 10.9 % | 11.1 % | 10.7 % | 11.1 % | 11.1 % | 11.2 % | 11.0 % |
| Franjas | 1 | 1 | 1 | 1 | 3 | 9 | 36 |
| Bytes por JTAG | 0.00 MB | 0.01 MB | 0.02 MB | 0.03 MB | 0.14 MB | 0.55 MB | 2.29 MB |
| Enlace JTAG (KB/s) | 257 | 280 | 278 | 270 | 266 | 274 | 268 |
| Enlace / máximo en bruto | 8.6 % | 9.3 % | 9.3 % | 9.0 % | 8.9 % | 9.1 % | 8.9 % |

### sobel

Por píxel de salida: 6 B leídos y 1 B escritos en la memoria interna, contra 1 B de entrada y 1 B de salida mínimos: la entrada se lee 6 veces (sin reutilización de la ventana).

|  | 32² | 64² | 100² | 128² | 256² | 512² | 1024² |
|---|---|---|---|---|---|---|---|
| Kernel, T / tiempo (MB/s) | 10.3 | 10.7 | 10.4 | 10.9 | 10.9 | 11.0 | 10.8 |
| Memoria interna (MB/s) | 36.0 | 37.5 | 36.6 | 38.2 | 38.2 | 38.6 | 37.8 |
| Uso del pico de la memoria interna | 9.0 % | 9.4 % | 9.1 % | 9.5 % | 9.6 % | 9.7 % | 9.5 % |
| Franjas | 1 | 1 | 1 | 1 | 3 | 9 | 35 |
| Bytes por JTAG | 0.00 MB | 0.01 MB | 0.02 MB | 0.03 MB | 0.13 MB | 0.54 MB | 2.22 MB |
| Enlace JTAG (KB/s) | 267 | 300 | 286 | 280 | 279 | 269 | 273 |
| Enlace / máximo en bruto | 8.9 % | 10.0 % | 9.5 % | 9.3 % | 9.3 % | 9.0 % | 9.1 % |

### Propuesta 2 (estimada)

Con los mismos bytes y los ciclos del modelo de pipeline, el uso del pico de la memoria interna sube:

| Filtro | Propuesta 1 (modelo) | Propuesta 2 (estimada) |
|---|---|---|
| gauss | 11.1 % | 27.0 % |
| sobel | 9.5 % | 25.5 % |
