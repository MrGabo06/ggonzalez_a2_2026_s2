# Recursos y Fmax del VPROC en la DE1-SoC

Dispositivo 5CSEMA5F31C6 (Cyclone V), 25.1std.0 Build 1129 10/21/2025 SC Lite Edition. Salen de los reportes del Fitter y del Timing Analyzer. Fmax es el peor caso de los modelos lentos (85 C y 0 C) para el reloj de 50 MHz.

## Por diseño

| | vproc_de1soc | vproc_jtag | vproc_jtag_stp |
|---|---|---|---|
| Descripción | Prueba autónoma en la tarjeta (sin JTAG) | JTAG, backend fpga (propuesta 1) | JTAG con SignalTap |
| ALMs | 4,878 / 32,070 (15 %) | 7,341 / 32,070 (23 %) | 7,733 / 32,070 (24 %) |
| Registros | 3,175 | 8,500 | 9,686 |
| Bits de memoria | 231,456 (6 %) | 540,672 (13 %) | 1,327,104 (33 %) |
| Bloques M10K | 32 / 397 (8 %) | 66 / 397 (17 %) | 162 / 397 (41 %) |
| Bloques DSP | 10 / 87 (11 %) | 10 / 87 (11 %) | 10 / 87 (11 %) |
| Pines | 67 | 67 | 67 |
| Fmax de CLOCK_50 (peor modelo) | 62.84 MHz | 63.04 MHz | 65.60 MHz |
| Fmax / 50 MHz | 1.26x | 1.26x | 1.31x |
| Slack de setup (peor) | +4.086 ns | +4.138 ns | +4.755 ns |
| Slack de hold (peor) | +0.172 ns | +0.164 ns | +0.140 ns |
| Compilado | Mon Nov 23 17:30:01 2026 | Fri Nov 20 19:04:31 2026 | Mon Nov 23 16:12:53 2026 |

## Por módulo

Instancias hasta el nivel de los submódulos del procesador. Los ALMs incluyen a los hijos.

### Desglose de `vproc_de1soc`

| Instancia | ALMs | % de los ALMs | Registros | Bits de memoria | M10K | DSP |
|---|---|---|---|---|---|---|
| `de1soc_top` | 4,878 | 100.0 % | 3,175 | 231,456 | 32 | 10 |
| `altsyncram:exp_rom_rtl_0` | 0 | 0.0 % | 0 | 32,768 | 4 | 0 |
| &nbsp;&nbsp;`altsyncram_gtd1:auto_generated` | 0 | 0.0 % | 0 | 32,768 | 4 | 0 |
| `altsyncram:in_rom_rtl_0` | 0 | 0.0 % | 0 | 34,848 | 8 | 0 |
| &nbsp;&nbsp;`altsyncram_ftd1:auto_generated` | 0 | 0.0 % | 0 | 34,848 | 8 | 0 |
| `altsyncram:prog_rom_rtl_0` | 0 | 0.0 % | 0 | 16,384 | 2 | 0 |
| &nbsp;&nbsp;`altsyncram_mrd1:auto_generated` | 0 | 0.0 % | 0 | 16,384 | 2 | 0 |
| `bin2bcd20:u_bcd` | 139 | 2.9 % | 0 | 0 | 0 | 0 |
| `vproc_core:u_core` | 4,653 | 95.4 % | 3,082 | 147,456 | 18 | 10 |
| &nbsp;&nbsp;lógica propia de `vproc_core` (sin submódulos) | 2,248 | 46.1 % | 3,067 | 0 | 0 | 0 |
| &nbsp;&nbsp;`vproc_alu:u_alu` | 165 | 3.4 % | 0 | 0 | 0 | 2 |
| &nbsp;&nbsp;`vproc_control:u_ctrl` | 7 | 0.1 % | 7 | 0 | 0 | 0 |
| &nbsp;&nbsp;`vproc_decoder:u_dec` | 10 | 0.2 % | 0 | 0 | 0 | 0 |
| &nbsp;&nbsp;`vproc_dmem:u_dmem` | 287 | 5.9 % | 8 | 131,072 | 16 | 0 |
| &nbsp;&nbsp;`vproc_imem:u_imem` | 0 | 0.0 % | 0 | 16,384 | 2 | 0 |
| &nbsp;&nbsp;`vproc_vector_unit:u_vu` | 1,937 | 39.7 % | 0 | 0 | 0 | 8 |
| `seg7` (6 displays) y `sld_*` (JTAG, SignalTap) | 21 y 0 | | | | | |

### Desglose de `vproc_jtag`

| Instancia | ALMs | % de los ALMs | Registros | Bits de memoria | M10K | DSP |
|---|---|---|---|---|---|---|
| `de1soc_jtag_top` | 7,341 | 100.0 % | 8,500 | 540,672 | 66 | 10 |
| `bin2bcd20:u_bcd` | 139 | 1.9 % | 0 | 0 | 0 | 0 |
| `vjtag_bridge:u_bridge` | 2,272 | 30.9 % | 5,402 | 0 | 0 | 0 |
| `vproc_core:u_core` | 4,848 | 66.0 % | 3,009 | 540,672 | 66 | 10 |
| &nbsp;&nbsp;lógica propia de `vproc_core` (sin submódulos) | 2,376 | 32.4 % | 2,995 | 0 | 0 | 0 |
| &nbsp;&nbsp;`vproc_alu:u_alu` | 180 | 2.4 % | 0 | 0 | 0 | 2 |
| &nbsp;&nbsp;`vproc_control:u_ctrl` | 6 | 0.1 % | 7 | 0 | 0 | 0 |
| &nbsp;&nbsp;`vproc_decoder:u_dec` | 10 | 0.1 % | 0 | 0 | 0 | 0 |
| &nbsp;&nbsp;`vproc_dmem:u_dmem` | 293 | 4.0 % | 7 | 524,288 | 64 | 0 |
| &nbsp;&nbsp;`vproc_imem:u_imem` | 0 | 0.0 % | 0 | 16,384 | 2 | 0 |
| &nbsp;&nbsp;`vproc_vector_unit:u_vu` | 1,983 | 27.0 % | 0 | 0 | 0 | 8 |
| `seg7` (6 displays) y `sld_*` (JTAG, SignalTap) | 21 y 54 | | | | | |

### Desglose de `vproc_jtag_stp`

| Instancia | ALMs | % de los ALMs | Registros | Bits de memoria | M10K | DSP |
|---|---|---|---|---|---|---|
| `de1soc_jtag_top` | 7,733 | 100.0 % | 9,686 | 1,327,104 | 162 | 10 |
| `bin2bcd20:u_bcd` | 131 | 1.7 % | 0 | 0 | 0 | 0 |
| `vjtag_bridge:u_bridge` | 2,316 | 29.9 % | 5,409 | 0 | 0 | 0 |
| `vproc_core:u_core` | 4,842 | 62.6 % | 2,947 | 540,672 | 66 | 10 |
| &nbsp;&nbsp;lógica propia de `vproc_core` (sin submódulos) | 2,370 | 30.6 % | 2,937 | 0 | 0 | 0 |
| &nbsp;&nbsp;`vproc_alu:u_alu` | 187 | 2.4 % | 0 | 0 | 0 | 2 |
| &nbsp;&nbsp;`vproc_control:u_ctrl` | 6 | 0.1 % | 4 | 0 | 0 | 0 |
| &nbsp;&nbsp;`vproc_decoder:u_dec` | 9 | 0.1 % | 0 | 0 | 0 | 0 |
| &nbsp;&nbsp;`vproc_dmem:u_dmem` | 296 | 3.8 % | 6 | 524,288 | 64 | 0 |
| &nbsp;&nbsp;`vproc_imem:u_imem` | 0 | 0.0 % | 0 | 16,384 | 2 | 0 |
| &nbsp;&nbsp;`vproc_vector_unit:u_vu` | 1,974 | 25.5 % | 0 | 0 | 0 | 8 |
| `seg7` (6 displays) y `sld_*` (JTAG, SignalTap) | 21 y 416 | | | | | |

## Lo que se puede leer

- **El procesador cabe con holgura.** `vproc_core` usa 4,848 ALMs (15.1 % del chip), 3,009 registros, 66 bloques M10K y 10 DSP. El puente JTAG agrega 2,272 ALMs y 5,402 registros, más que el procesador en registros, probablemente por los registros de 1064 bits de cada scan JTAG.
- **Margen de reloj.** Fmax de 63.0 MHz contra los 50 MHz del diseño: 26 % de margen. Una propuesta con pipeline y forwarding alarga la ruta crítica, y ese margen es lo que se gastaría.
- **Costo de SignalTap:** +392 ALMs, +1,186 registros, +96 bloques M10K y +786,432 bits de memoria (32K muestras de 24 bits). Fmax pasa de 63.0 a 65.6 MHz.
- **Diseño de la tarjeta frente al de JTAG:** 4,878 contra 7,341 ALMs. La diferencia es el puente JTAG y la memoria de datos de 64 KB (la del diseño de la tarjeta es de 16 KB y trae el programa, la imagen y la salida esperada en ROM).
- **Los DSP** son los multiplicadores 18×18: 8 de la unidad vectorial (uno por carril) y 2 de la ALU escalar.
