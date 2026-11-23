# Eficiencia paralela de CUDA y de la FPGA

## Definiciones

- **Speedup** S = mediana de `seq` / tiempo del dispositivo, mismo filtro, tamaño y máquina. En la GPU y la FPGA se usa solo el kernel (cudaEvent y contador de ciclos), sin copias ni JTAG.
- **Eficiencia paralela clásica** E = S / p, con p el número de carriles paralelos del dispositivo. En SIMD, `analyze.py` la calcula contra su secuencial dividiendo entre 8 o 16 píxeles por instrucción.
- **Utilización del pico** E_peak = MAC útiles / (ciclos x carriles). No depende del reloj y mide qué parte de la capacidad de cómputo hace trabajo útil. MAC útiles = píxeles x taps no nulos (Gauss 25, Sobel 6).
- **Cómo leerlas.** E clásica compara contra un núcleo de CPU que corre a un reloj distinto, así que no es una fracción de la capacidad del dispositivo. E_peak sí lo es. Una E clásica baja con un speedup alto es normal en una GPU, donde p es de miles de carriles.

## CUDA: eficiencia paralela clásica (E = S / p)

S es la mediana de `seq` entre el tiempo del kernel en la GPU (sin copias), en la misma máquina. Cada celda es `S / E`.

### gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE

GPU con 20 SMs, capacidad 8.6: p = 20 x 128 = 2560 carriles FP32.

| Backend | Filtro | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|---|
| cuda | gauss | 106x / 4.15 % | 135x / 5.26 % | 141x / 5.49 % | 142x / 5.55 % |
| cuda | sobel | 109x / 4.25 % | 179x / 7.01 % | 186x / 7.27 % | 190x / 7.44 % |
| cuda_tiled | gauss | 142x / 5.56 % | 186x / 7.25 % | 194x / 7.59 % | 198x / 7.75 % |
| cuda_tiled | sobel | 93x / 3.62 % | 134x / 5.23 % | 147x / 5.73 % | 152x / 5.93 % |

### msi-ema

GPU con 30 SMs, capacidad 8.6: p = 30 x 128 = 3840 carriles FP32.

| Backend | Filtro | 512² | 1024² | 2048² | 4096² |
|---|---|---|---|---|---|
| cuda | gauss | 174x / 4.54 % | 228x / 5.93 % | 234x / 6.09 % | 232x / 6.04 % |
| cuda | sobel | 193x / 5.02 % | 291x / 7.58 % | 325x / 8.47 % | 335x / 8.74 % |
| cuda_tiled | gauss | 299x / 7.78 % | 375x / 9.76 % | 383x / 9.97 % | 384x / 10.0 % |
| cuda_tiled | sobel | 152x / 3.95 % | 208x / 5.41 % | 246x / 6.40 % | 250x / 6.52 % |

## CUDA: utilización del pico a 4096² (ciclos de Nsight Compute)

E_peak = (píxeles x taps no nulos) / (ciclos transcurridos x SMs x carriles FP32 por SM). Un FMA cuenta como una MAC. Los ciclos no dependen del reloj, que Nsight fija. `SM Throughput` e `IPC` son los de Nsight, para comparar.

| Máquina | Backend | Filtro | MAC útiles | Ciclos | E_peak | GMAC/s (kernel real) | SM Throughput | IPC |
|---|---|---|---|---|---|---|---|---|
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | cuda | gauss | 419 M | 4.14 M | 3.96 % | 171 | 92 % | 3.26 |
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | cuda | sobel | 101 M | 1.42 M | 2.77 % | 117 | 78 % | 3.04 |
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | cuda_tiled | gauss | 419 M | 2.95 M | 5.56 % | 239 | 86 % | 3.44 |
| gabriel-ASUS-TUF-Gaming-A15-FA507RE-FA507RE | cuda_tiled | sobel | 101 M | 1.80 M | 2.19 % | 93 | 81 % | 3.25 |
| msi-ema | cuda | gauss | 419 M | 2.75 M | 3.97 % | 221 | 93 % | 3.27 |
| msi-ema | cuda | sobel | 101 M | 0.91 M | 2.88 % | 177 | 81 % | 3.15 |
| msi-ema | cuda_tiled | gauss | 419 M | 1.95 M | 5.60 % | 366 | 86 % | 3.46 |
| msi-ema | cuda_tiled | sobel | 101 M | 1.18 M | 2.22 % | 132 | 82 % | 3.30 |

## FPGA (VPROC de 8 carriles a 50 MHz)

- Clásica: S es la mediana de `seq` en chunchunmaru (la CPU que maneja la tarjeta) entre el tiempo de kernel (ciclos / 50 MHz); p = 8. Compara un reloj de 50 MHz contra el de la CPU, así que E queda muy por debajo de 1.
- E_peak = (píxeles x taps no nulos) / (ciclos x 8). Con los ciclos del contador de hardware. Es la fracción de ciclos en que los 8 carriles hacen una MAC útil.
- Estimada: ciclos de la propuesta 2 (pipeline) según `fpga/scripts/pipeline_model.py`, no medidos.

### gauss

| | 32² | 64² | 100² | 128² | 256² | 512² |
|---|---|---|---|---|---|---|
| S (kernel contra seq) | 0.044x | 0.046x | 0.043x | 0.046x | 0.048x | 0.047x |
| E clásica | 0.55 % | 0.57 % | 0.54 % | 0.58 % | 0.60 % | 0.58 % |
| GMAC/s | 0.042 | 0.043 | 0.041 | 0.043 | 0.043 | 0.043 |
| E_peak, propuesta 1 (medida) | 10.5 % | 10.7 % | 10.3 % | 10.7 % | 10.7 % | 10.7 % |
| E_peak, propuesta 2 (estimada) | 25.5 % | 25.8 % | 24.9 % | 25.9 % | 26.0 % | 26.0 % |

### sobel

| | 32² | 64² | 100² | 128² | 256² | 512² |
|---|---|---|---|---|---|---|
| S (kernel contra seq) | 0.060x | 0.063x | 0.062x | 0.065x | 0.065x | 0.065x |
| E clásica | 0.75 % | 0.79 % | 0.78 % | 0.81 % | 0.81 % | 0.81 % |
| GMAC/s | 0.031 | 0.032 | 0.031 | 0.033 | 0.033 | 0.033 |
| E_peak, propuesta 1 (medida) | 7.72 % | 8.03 % | 7.83 % | 8.18 % | 8.19 % | 8.28 % |
| E_peak, propuesta 2 (estimada) | 20.8 % | 21.5 % | 20.9 % | 21.9 % | 22.0 % | 22.1 % |
