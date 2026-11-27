# Filtro Sobel 3x3 en dirección x para el procesador vectorial VPROC:
#   [-1 0 1]
#   [-2 0 2]   = [1 2 1]^T x [-1 0 1]
#   [-1 0 1]
# Calcula LANES (8) píxeles de salida por iteración.
#
# Constantes que entrega el host (-D en vasm.py):
#   W, H  tamaño de la imagen de salida (W múltiplo de 8)
#   WP    ancho de la imagen de entrada con borde replicado (W + 2)
#   IN    dirección de la imagen con borde ((W+2) x (H+2) bytes)
#   OUT   dirección de la imagen de salida (W x H bytes)
#
# Por grupo de 8 píxeles:
#   para cada fila i de la ventana:  d = pix[y+i][x+2 ..] - pix[y+i][x ..]
#   acc += c_i * d     con c = 1, 2, 1
#   salida = sat(|acc|)   (VABS y la saturación de VST)
#
# Registros:
#   r1 fila y de entrada   r2 fila y de salida   r3 filas restantes   r4 x
#   r5 entrada en (x, y)   r6 salida en (x, y)   r7 W   r8 fila de la ventana
#   r10 = 1, r11 = 2 (pesos verticales)   r13 = WP
#   v0 acumulador   v1, v2 píxeles leídos   v3 diferencia

        LI    r1, IN
        LI    r2, OUT
        LI    r3, H
        LI    r7, W
        LI    r13, WP
        ADDI  r10, r0, 1
        ADDI  r11, r0, 2

row:    ADDI  r4, r0, 0
        MOV   r5, r1
        MOV   r6, r2

col:    VBCAST v0, r0               # acc = 0
        MOV   r8, r5
        # fila 0 (peso 1)
        VLD   v1, 0(r8)
        VLD   v2, 2(r8)
        VSUB  v3, v2, v1
        VMACS v0, v3, r10
        ADD   r8, r8, r13
        # fila 1 (peso 2)
        VLD   v1, 0(r8)
        VLD   v2, 2(r8)
        VSUB  v3, v2, v1
        VMACS v0, v3, r11
        ADD   r8, r8, r13
        # fila 2 (peso 1)
        VLD   v1, 0(r8)
        VLD   v2, 2(r8)
        VSUB  v3, v2, v1
        VMACS v0, v3, r10

        VABS  v0, v0
        VST   v0, 0(r6)
        ADDI  r4, r4, 8
        ADDI  r5, r5, 8
        ADDI  r6, r6, 8
        BLT   r4, r7, col

        ADD   r1, r1, r13
        ADD   r2, r2, r7
        ADDI  r3, r3, -1
        BNE   r3, r0, row
        HALT
