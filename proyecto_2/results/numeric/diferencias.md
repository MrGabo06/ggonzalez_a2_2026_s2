# Diferencias numéricas entre CPU, GPU y FPGA

## Comparación directa (digest SHA-256 de las salidas)

| Caso | Filtro | Tamaño | Resultado |
|---|---|---|---|
| CPU + GPU | gauss | 64² (semilla 42) | pendiente: falta GPU (correr `make numeric_export` en esa máquina) |
| CPU + FPGA | gauss | 64² (semilla 42) | **idénticos bit a bit** (diferencia máxima 0, 0 píxeles distintos; 7 salidas) |
| GPU + CPU + FPGA | gauss | 64² (semilla 42) | pendiente: falta GPU (correr `make numeric_export` en esa máquina) |
| CPU + GPU | gauss | 512² (semilla 42) | pendiente: falta GPU (correr `make numeric_export` en esa máquina) |
| CPU + FPGA | gauss | 512² (semilla 42) | **idénticos bit a bit** (diferencia máxima 0, 0 píxeles distintos; 7 salidas) |
| GPU + CPU + FPGA | gauss | 512² (semilla 42) | pendiente: falta GPU (correr `make numeric_export` en esa máquina) |
| CPU + GPU | sobel | 64² (semilla 42) | pendiente: falta GPU (correr `make numeric_export` en esa máquina) |
| CPU + FPGA | sobel | 64² (semilla 42) | **idénticos bit a bit** (diferencia máxima 0, 0 píxeles distintos; 7 salidas) |
| GPU + CPU + FPGA | sobel | 64² (semilla 42) | pendiente: falta GPU (correr `make numeric_export` en esa máquina) |
| CPU + GPU | sobel | 512² (semilla 42) | pendiente: falta GPU (correr `make numeric_export` en esa máquina) |
| CPU + FPGA | sobel | 512² (semilla 42) | **idénticos bit a bit** (diferencia máxima 0, 0 píxeles distintos; 7 salidas) |
| GPU + CPU + FPGA | sobel | 512² (semilla 42) | pendiente: falta GPU (correr `make numeric_export` en esa máquina) |

### Inventario de digests

| Máquina | Backend | Filtro | Tamaño | entrada | salida |
|---|---|---|---|---|---|
| chunchunmaru | fpga | gauss | 64² | `4609900127` | `bcc8c6adb1` |
| chunchunmaru | seq | gauss | 64² | `4609900127` | `bcc8c6adb1` |
| chunchunmaru | seq_sep | gauss | 64² | `4609900127` | `bcc8c6adb1` |
| chunchunmaru | simd | gauss | 64² | `4609900127` | `bcc8c6adb1` |
| chunchunmaru | simd512 | gauss | 64² | `4609900127` | `bcc8c6adb1` |
| chunchunmaru | simd_sep | gauss | 64² | `4609900127` | `bcc8c6adb1` |
| chunchunmaru | simd_sep512 | gauss | 64² | `4609900127` | `bcc8c6adb1` |
| chunchunmaru | fpga | gauss | 512² | `d1537f64fa` | `463265710a` |
| chunchunmaru | seq | gauss | 512² | `d1537f64fa` | `463265710a` |
| chunchunmaru | seq_sep | gauss | 512² | `d1537f64fa` | `463265710a` |
| chunchunmaru | simd | gauss | 512² | `d1537f64fa` | `463265710a` |
| chunchunmaru | simd512 | gauss | 512² | `d1537f64fa` | `463265710a` |
| chunchunmaru | simd_sep | gauss | 512² | `d1537f64fa` | `463265710a` |
| chunchunmaru | simd_sep512 | gauss | 512² | `d1537f64fa` | `463265710a` |
| chunchunmaru | fpga | sobel | 64² | `4609900127` | `4fc53b2832` |
| chunchunmaru | seq | sobel | 64² | `4609900127` | `4fc53b2832` |
| chunchunmaru | seq_sep | sobel | 64² | `4609900127` | `4fc53b2832` |
| chunchunmaru | simd | sobel | 64² | `4609900127` | `4fc53b2832` |
| chunchunmaru | simd512 | sobel | 64² | `4609900127` | `4fc53b2832` |
| chunchunmaru | simd_sep | sobel | 64² | `4609900127` | `4fc53b2832` |
| chunchunmaru | simd_sep512 | sobel | 64² | `4609900127` | `4fc53b2832` |
| chunchunmaru | fpga | sobel | 512² | `d1537f64fa` | `72cc749412` |
| chunchunmaru | seq | sobel | 512² | `d1537f64fa` | `72cc749412` |
| chunchunmaru | seq_sep | sobel | 512² | `d1537f64fa` | `72cc749412` |
| chunchunmaru | simd | sobel | 512² | `d1537f64fa` | `72cc749412` |
| chunchunmaru | simd512 | sobel | 512² | `d1537f64fa` | `72cc749412` |
| chunchunmaru | simd_sep | sobel | 512² | `d1537f64fa` | `72cc749412` |
| chunchunmaru | simd_sep512 | sobel | 512² | `d1537f64fa` | `72cc749412` |

## Derivado de la validación contra seq

Cada backend se validó contra `seq` con el mismo filtro, tamaño y semilla. Como |A − B| ≤ |A − seq| + |B − seq|, el error entre dos plataformas está acotado por la suma de sus errores contra `seq`. Solo vale si la imagen y la salida de `seq` son iguales en todas las máquinas. Eso lo confirman los digests, y hoy hay digests de: chunchunmaru. Las filas que mezclan máquinas sin digest propio son derivadas, no verificadas.

| Caso | Filtro | Tamaño | Error de cada clase | Cota de la diferencia |
|---|---|---|---|---|
| CPU + GPU | gauss | 512² | CPU 0, GPU 0 | 0 |
| CPU + GPU | gauss | 1024² | CPU 0, GPU 0 | 0 |
| CPU + GPU | gauss | 2048² | CPU 0, GPU 0 | 0 |
| CPU + GPU | gauss | 4096² | CPU 0, GPU 0 | 0 |
| CPU + GPU | sobel | 512² | CPU 0, GPU 0 | 0 |
| CPU + GPU | sobel | 1024² | CPU 0, GPU 0 | 0 |
| CPU + GPU | sobel | 2048² | CPU 0, GPU 0 | 0 |
| CPU + GPU | sobel | 4096² | CPU 0, GPU 0 | 0 |
| CPU + FPGA | gauss | 32² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | gauss | 64² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | gauss | 100² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | gauss | 128² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | gauss | 256² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | gauss | 512² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | sobel | 32² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | sobel | 64² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | sobel | 100² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | sobel | 128² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | sobel | 256² | CPU 0, FPGA 0 | 0 |
| CPU + FPGA | sobel | 512² | CPU 0, FPGA 0 | 0 |
| GPU + CPU + FPGA | gauss | 512² | GPU 0, CPU 0, FPGA 0 | 0 |
| GPU + CPU + FPGA | sobel | 512² | GPU 0, CPU 0, FPGA 0 | 0 |

## Por qué la diferencia es 0

CPU y GPU calculan en `float32` y la FPGA en enteros, así que el resultado idéntico no es obvio. Se
cumple porque todos los pesos son múltiplos de 1/256 (Gauss: `k/256` con k entero; Sobel: enteros) y
las entradas son bytes. Cada producto y cada suma parcial es un múltiplo de 1/256 con valor absoluto
menor que 1024 (Sobel llega a ±4·255 = ±1020 antes del valor absoluto), así que necesita como máximo
18 bits significativos (10 enteros y 8 fraccionarios), menos que los 24 de la mantisa de `float32`:
se representa sin redondeo. Por eso el orden de la suma (secuencial, SIMD, hilos de la GPU) y el uso
de FMA no cambian el resultado. El redondeo
final del baseline, `trunc(v + 0.5)`, coincide con el `(acc + 128) >> 8` de la FPGA para valores
no negativos, y Sobel aplica valor absoluto y saturación igual en todos.
