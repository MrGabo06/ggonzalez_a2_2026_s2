# Prueba de todas las instrucciones del ISA. Cada resultado se replica en los 8 carriles
# con VBCAST y se guarda con VST; el script compara los 64 bytes de salida (8 grupos)
# contra los valores esperados de abajo.
        LI     r1, OUT
        ADDI   r2, r0, 7
        ADDI   r3, r0, 5
        SUB    r4, r2, r3          # 2
        MUL    r5, r2, r3          # 35
        SLLI   r6, r3, 4           # 80
        JMP    skip
        ADDI   r6, r0, 0           # no se ejecuta
skip:   BLT    r3, r2, ok          # 5 < 7: salta
        ADDI   r6, r0, 1           # no se ejecuta
ok:     BNE    r0, r0, bad         # no salta
        NOP
        VBCAST v0, r4
        VST    v0, 0(r1)           # grupo 0: 2
        VBCAST v1, r5
        VST    v1, 8(r1)           # grupo 1: 35
        VBCAST v2, r6
        VST    v2, 16(r1)          # grupo 2: 80
        VSUB   v3, v0, v1          # -33
        VABS   v7, v3
        VST    v7, 24(r1)          # grupo 3: 33
        VST    v3, 32(r1)          # grupo 4: -33 saturado a 0
        VMULS  v4, v1, r6          # 2800
        VSRAI  v4, v4, 4           # 175
        VST    v4, 40(r1)          # grupo 5: 175
        VADDI  v5, v4, 100         # 275
        VST    v5, 48(r1)          # grupo 6: 275 saturado a 255
        VBCAST v6, r2
        VMACS  v6, v0, r3          # 7 + 2 * 5 = 17
        VADD   v6, v6, v0          # 19
        VST    v6, 56(r1)          # grupo 7: 19
        VLD    v7, 8(r1)           # relee el grupo 1 (35) desde memoria
        RDCYC  r9
        BLT    r0, r9, end         # el contador avanzó: termina bien
bad:    VST    v3, 0(r1)           # marca error en el grupo 0
end:    HALT
