# Prueba mínima del núcleo ("ejecución dummy"): suma 1 a 16 píxeles y mide ciclos.
# Toca todas las clases de instrucción: escalar, salto, VLD, operación vectorial, VST, RDCYC.
#   entrada: 16 bytes en IN       salida: 16 bytes en OUT = min(entrada + 1, 255)
        LI     r1, IN
        LI     r2, OUT
        ADDI   r3, r0, 2            # 2 grupos de 8 píxeles
        ADDI   r5, r0, 1
loop:   VLD    v0, 0(r1)
        VBCAST v1, r5
        VADD   v0, v0, v1
        VST    v0, 0(r2)
        ADDI   r1, r1, 8
        ADDI   r2, r2, 8
        ADDI   r3, r3, -1
        BNE    r3, r0, loop
        RDCYC  r9
        HALT
