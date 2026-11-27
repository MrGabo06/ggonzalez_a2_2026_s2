# Filtro Gaussiano 5x5 (aproximación binomial [1 4 6 4 1]^T x [1 4 6 4 1] / 256)
# para el procesador vectorial VPROC. Calcula LANES (8) píxeles de salida por iteración.
#
# Constantes que entrega el host (-D en vasm.py):
#   W, H  tamaño de la imagen de salida (W múltiplo de 8)
#   WP    ancho de la imagen de entrada con borde replicado (W + 4)
#   IN    dirección de la imagen con borde en la memoria de datos ((W+4) x (H+4) bytes)
#   OUT   dirección de la imagen de salida (W x H bytes)
#
# Cálculo por grupo de 8 píxeles (separable, todo en registros):
#   para cada fila i de la ventana:  fila = sum_j k_j * pix[y+i][x+j .. x+j+7]
#   acc += k_i * fila
#   salida = (acc + 128) >> 8          (mismo redondeo que el baseline: +0.5 y truncar)
#
# Registros:
#   r1 puntero a la fila y de entrada   r2 puntero a la fila y de salida   r3 filas restantes
#   r4 x                                r5 entrada en (x, y)               r6 salida en (x, y)
#   r7 W                                r8 fila actual de la ventana
#   r10 = 1, r11 = 4, r12 = 6 (pesos)   r13 = WP (salto de una fila)
#   v0 acumulador   v1 píxeles leídos   v2 suma horizontal de una fila

        LI    r1, IN
        LI    r2, OUT
        LI    r3, H
        LI    r7, W
        LI    r13, WP
        ADDI  r10, r0, 1
        ADDI  r11, r0, 4
        ADDI  r12, r0, 6

row:    ADDI  r4, r0, 0             # x = 0
        MOV   r5, r1
        MOV   r6, r2

col:    VBCAST v0, r0               # acc = 0
        MOV   r8, r5
        # fila 0 de la ventana (peso vertical k0 = 1)
        VLD   v1, 0(r8)
        VMULS v2, v1, r10
        VLD   v1, 1(r8)
        VMACS v2, v1, r11
        VLD   v1, 2(r8)
        VMACS v2, v1, r12
        VLD   v1, 3(r8)
        VMACS v2, v1, r11
        VLD   v1, 4(r8)
        VMACS v2, v1, r10
        VMACS v0, v2, r10        # acc += k0 * fila
        ADD   r8, r8, r13          # siguiente fila de la ventana
        # fila 1 de la ventana (peso vertical k1 = 4)
        VLD   v1, 0(r8)
        VMULS v2, v1, r10
        VLD   v1, 1(r8)
        VMACS v2, v1, r11
        VLD   v1, 2(r8)
        VMACS v2, v1, r12
        VLD   v1, 3(r8)
        VMACS v2, v1, r11
        VLD   v1, 4(r8)
        VMACS v2, v1, r10
        VMACS v0, v2, r11        # acc += k1 * fila
        ADD   r8, r8, r13          # siguiente fila de la ventana
        # fila 2 de la ventana (peso vertical k2 = 6)
        VLD   v1, 0(r8)
        VMULS v2, v1, r10
        VLD   v1, 1(r8)
        VMACS v2, v1, r11
        VLD   v1, 2(r8)
        VMACS v2, v1, r12
        VLD   v1, 3(r8)
        VMACS v2, v1, r11
        VLD   v1, 4(r8)
        VMACS v2, v1, r10
        VMACS v0, v2, r12        # acc += k2 * fila
        ADD   r8, r8, r13          # siguiente fila de la ventana
        # fila 3 de la ventana (peso vertical k3 = 4)
        VLD   v1, 0(r8)
        VMULS v2, v1, r10
        VLD   v1, 1(r8)
        VMACS v2, v1, r11
        VLD   v1, 2(r8)
        VMACS v2, v1, r12
        VLD   v1, 3(r8)
        VMACS v2, v1, r11
        VLD   v1, 4(r8)
        VMACS v2, v1, r10
        VMACS v0, v2, r11        # acc += k3 * fila
        ADD   r8, r8, r13          # siguiente fila de la ventana
        # fila 4 de la ventana (peso vertical k4 = 1)
        VLD   v1, 0(r8)
        VMULS v2, v1, r10
        VLD   v1, 1(r8)
        VMACS v2, v1, r11
        VLD   v1, 2(r8)
        VMACS v2, v1, r12
        VLD   v1, 3(r8)
        VMACS v2, v1, r11
        VLD   v1, 4(r8)
        VMACS v2, v1, r10
        VMACS v0, v2, r10        # acc += k4 * fila
        ADD   r8, r8, r13          # siguiente fila de la ventana
        VADDI v0, v0, 128           # redondeo
        VSRAI v0, v0, 8             # / 256
        VST   v0, 0(r6)             # satura a 0..255 y guarda 8 píxeles
        ADDI  r4, r4, 8
        ADDI  r5, r5, 8
        ADDI  r6, r6, 8
        BLT   r4, r7, col

        ADD   r1, r1, r13           # siguiente fila de entrada
        ADD   r2, r2, r7            # siguiente fila de salida
        ADDI  r3, r3, -1
        BNE   r3, r0, row
        HALT
