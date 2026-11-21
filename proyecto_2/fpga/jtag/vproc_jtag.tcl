# Comunicación con el VPROC en la DE1-SoC por Virtual JTAG (fpga/jtag/de1soc_jtag_top.v).
#
# Uso:
#   quartus_stp -t fpga/jtag/vproc_jtag.tcl ping      # prueba rápida: ¿está el puente?
#   quartus_stp -t fpga/jtag/vproc_jtag.tcl server    # lo usa el backend fpga de dlp_bench
#
# En modo server lee comandos uno por línea y responde con una línea que empieza con "@@".
# Si existen VPROC_JTAG_CMD y VPROC_JTAG_REPLY (dos FIFOs que crea el backend) los usa en lugar
# de la entrada y la salida estándar: quartus_stp retiene su salida estándar cuando no es una
# terminal, y así las respuestas llegan de inmediato. Sin esas variables usa stdin/stdout:
#   PING                        -> @@ OK <seq> <busy> <done> <error> <ciclos>
#   PROG <archivo>              -> @@ OK <palabras>    programa: una palabra hex de 32 bits por línea
#   WRITE <dir> <archivo>       -> @@ OK <bytes>       datos: un byte hex por línea
#   RUN <timeout_s>             -> @@ OK <ciclos> <error>
#   READ <dir> <n> <archivo>    -> @@ OK <bytes>       escribe un byte hex por línea
#   RESET                       -> @@ OK
#   QUIT                        -> @@ BYE
# Si algo falla responde "@@ ERR <mensaje>".
#
# Para simular sin tarjeta (make fpga_jtag_sim) se usa tclsh y las variables de entorno
# VPROC_JTAG_SIM_REQ y VPROC_JTAG_SIM_RESP: los scans van por dos FIFOs a Icarus Verilog,
# que simula el mismo top con un modelo del Virtual JTAG (fpga/jtag/sim/).
#
# Opcionales: VPROC_JTAG_HW (nombre del cable, p. ej. "DE-SoC [3-1]") y VPROC_JTAG_DEV
# (nombre del dispositivo) si hay más de una tarjeta o la detección automática falla.

set BURST 128                       ;# bytes por scan, igual que en de1soc_jtag_top.v
set DRW   [expr {40 + 8 * $BURST}]  ;# bits del registro de datos
set NHEX  [expr {$DRW / 4}]
set IR_CMD 1

set OP_NOP 0; set OP_WIMEM 1; set OP_WDMEM 2; set OP_RDMEM 3
set OP_START 4; set OP_STATUS 5; set OP_RESET 6

# ======================================================================= transporte
if {[info exists ::env(VPROC_JTAG_SIM_REQ)]} {
    # ------------------------------------------------ simulación (FIFOs hacia Icarus)
    proc jt_open {} {
        set ::sim_req  [open $::env(VPROC_JTAG_SIM_REQ) w]
        set ::sim_resp [open $::env(VPROC_JTAG_SIM_RESP) r]
        fconfigure $::sim_req -buffering line
        return "simulación"
    }
    proc jt_ir {v} {
        puts $::sim_req "IR $v"
        gets $::sim_resp
    }
    proc jt_dr {hex} {
        puts $::sim_req "DR $::DRW $hex"
        set r [string trim [gets $::sim_resp]]
        return [string toupper [string repeat 0 [expr {$::NHEX - [string length $r]}]]$r]
    }
    proc jt_close {} {
        catch {puts $::sim_req "Q"}
        catch {close $::sim_req}
        catch {close $::sim_resp}
    }
} else {
    # ------------------------------------------------ tarjeta real (quartus_stp)
    proc jt_open {} {
        set hws [get_hardware_names]
        if {[llength $hws] == 0} { error "no hay cables JTAG (revise el USB y jtagconfig)" }
        if {[info exists ::env(VPROC_JTAG_HW)]} {
            set hw $::env(VPROC_JTAG_HW)
        } else {
            set hw [lindex $hws 0]
        }
        set dev ""
        if {[info exists ::env(VPROC_JTAG_DEV)]} {
            set dev $::env(VPROC_JTAG_DEV)
        } else {
            # La DE1-SoC tiene dos dispositivos en la cadena: la HPS (SOCVHPS) y la FPGA (5CSE...)
            foreach d [get_device_names -hardware_name $hw] {
                if {![string match "*SOCVHPS*" $d] && [string match "*5C*" $d]} { set dev $d }
            }
        }
        if {$dev eq ""} { error "no se encontró la FPGA en $hw" }
        open_device -hardware_name $hw -device_name $dev
        device_lock -timeout 10000
        return "$hw / $dev"
    }
    proc jt_ir {v} {
        device_virtual_ir_shift -instance_index 0 -ir_value $v -no_captured_ir_value
    }
    proc jt_dr {hex} {
        set r [device_virtual_dr_shift -instance_index 0 -length $::DRW -dr_value $hex -value_in_hex]
        return [string toupper [string repeat 0 [expr {$::NHEX - [string length $r]}]]$r]
    }
    proc jt_close {} {
        catch {device_unlock}
        catch {close_device}
    }
}

# ======================================================================= protocolo
# Un scan manda un comando y devuelve la respuesta del comando de DOS scans antes (ver
# vjtag_bridge.v). pend guarda {op bytes_a_leer} de los comandos cuya respuesta no ha llegado,
# y last_seq es el seq de la última respuesta recibida (la siguiente debe traer last_seq + 1).
set last_seq -1
set pend {}

# Arma el valor hexadecimal del DR: el byte 0 de datos queda justo después del encabezado.
proc pack {op cnt addr bytes} {
    set n [llength $bytes]
    set pad [string repeat 00 [expr {$::BURST - $n}]]
    set data ""
    for {set i [expr {$n - 1}]} {$i >= 0} {incr i -1} {
        append data [format %02X [lindex $bytes $i]]
    }
    return "$pad$data[format %06X%02X%02X [expr {$addr & 0xFFFFFF}] [expr {$cnt & 0xFF}] $op]"
}

# Separa la respuesta en un diccionario. bytes es la lista de los primeros n bytes de datos.
proc unpack {hex {n 0}} {
    set L [string length $hex]
    set h(magic)  [scan [string range $hex [expr {$L - 2}]  [expr {$L - 1}]] %x]
    set h(seq)    [scan [string range $hex [expr {$L - 4}]  [expr {$L - 3}]] %x]
    set st        [scan [string range $hex [expr {$L - 6}]  [expr {$L - 5}]] %x]
    set h(op)     [scan [string range $hex [expr {$L - 8}]  [expr {$L - 7}]] %x]
    set h(burst)  [scan [string range $hex [expr {$L - 10}] [expr {$L - 9}]] %x]
    set h(error)   [expr {$st & 1}]
    set h(done)    [expr {($st >> 1) & 1}]
    set h(busy)    [expr {($st >> 2) & 1}]
    set h(cmd_err) [expr {($st >> 3) & 1}]
    set h(bytes) {}
    for {set i 0} {$i < $n} {incr i} {
        set e [expr {$L - 10 - 2 * $i}]
        lappend h(bytes) [scan [string range $hex [expr {$e - 2}] [expr {$e - 1}]] %x]
    }
    return [array get h]
}

# Manda un comando (nread: bytes de datos que traerá su respuesta) y devuelve la respuesta que
# llegó en este scan, que es la del comando de dos scans antes.
proc scan_cmd {op cnt addr bytes {nread 0}} {
    set nprev 0
    set opprev -1
    if {[llength $::pend] >= 2} {
        lassign [lindex $::pend 0] opprev nprev
        set ::pend [lrange $::pend 1 end]
    }
    set rd [dict create {*}[unpack [jt_dr [pack $op $cnt $addr $bytes]] $nprev]]
    lappend ::pend [list $op $nread]
    if {$::last_seq >= 0 && $opprev >= 0} {
        if {[dict get $rd magic] != 0xA5} {
            error "respuesta sin la marca 0xA5: ¿está programado de1soc_jtag_top?"
        }
        set want [expr {($::last_seq + 1) % 256}]
        if {[dict get $rd seq] != $want || [dict get $rd op] != $opprev} {
            error "respuesta desfasada (seq [dict get $rd seq] op [dict get $rd op], se esperaba seq $want op $opprev)"
        }
        if {[dict get $rd cmd_err]} {
            error "el núcleo estaba ocupado y rechazó el comando [dict get $rd op]"
        }
    }
    if {$opprev >= 0} { set ::last_seq [dict get $rd seq] }
    return $rd
}

# Dos NOP: la última respuesta que devuelve es la del último comando real.
proc flush_cmds {} {
    scan_cmd $::OP_NOP 0 0 {}
    return [scan_cmd $::OP_NOP 0 0 {}]
}

# Sincroniza con el puente: el tercer NOP trae la respuesta del primero, con la marca.
proc sync {} {
    set ::last_seq -1
    set ::pend {}
    jt_ir $::IR_CMD
    scan_cmd $::OP_NOP 0 0 {}
    scan_cmd $::OP_NOP 0 0 {}
    set r [scan_cmd $::OP_NOP 0 0 {}]
    if {[dict get $r magic] != 0xA5} {
        error "el puente JTAG no responde (¿se programó el .sof de fpga/jtag?)"
    }
}

proc status {} {
    scan_cmd $::OP_STATUS 0 0 {} 4
    set r [flush_cmds]
    set b [dict get $r bytes]
    set cyc [expr {[lindex $b 0] | ([lindex $b 1] << 8) | ([lindex $b 2] << 16) | ([lindex $b 3] << 24)}]
    dict set r cycles $cyc
    return $r
}

proc read_lines {path} {
    set f [open $path r]
    set out {}
    while {[gets $f line] >= 0} {
        set line [string trim $line]
        if {$line ne ""} { lappend out [scan $line %x] }
    }
    close $f
    return $out
}

proc cmd_prog {path} {
    set words [read_lines $path]
    set per [expr {$::BURST / 4}]
    for {set i 0} {$i < [llength $words]} {incr i $per} {
        set chunk [lrange $words $i [expr {$i + $per - 1}]]
        set bytes {}
        foreach w $chunk {
            lappend bytes [expr {$w & 0xFF}] [expr {($w >> 8) & 0xFF}] \
                          [expr {($w >> 16) & 0xFF}] [expr {($w >> 24) & 0xFF}]
        }
        scan_cmd $::OP_WIMEM [llength $chunk] $i $bytes
    }
    flush_cmds
    return [llength $words]
}

proc cmd_write {addr path} {
    set data [read_lines $path]
    for {set i 0} {$i < [llength $data]} {incr i $::BURST} {
        set chunk [lrange $data $i [expr {$i + $::BURST - 1}]]
        scan_cmd $::OP_WDMEM [llength $chunk] [expr {$addr + $i}] $chunk
    }
    flush_cmds
    return [llength $data]
}

proc cmd_read {addr n path} {
    set f [open $path w]
    set got 0
    set scans {}
    for {set i 0} {$i < $n} {incr i $::BURST} {
        lappend scans [list $::OP_RDMEM [expr {min($::BURST, $n - $i)}] [expr {$addr + $i}]]
    }
    lappend scans [list $::OP_NOP 0 0] [list $::OP_NOP 0 0]
    foreach sc $scans {
        lassign $sc op cnt a
        set r [scan_cmd $op $cnt $a {} [expr {$op == $::OP_RDMEM ? $cnt : 0}]]
        if {[dict get $r op] == $::OP_RDMEM && $got < $n} {
            foreach b [dict get $r bytes] { puts $f [format %02X $b] }
            incr got [llength [dict get $r bytes]]
        }
    }
    close $f
    if {$got != $n} { error "se leyeron $got de $n bytes" }
    return $n
}

proc cmd_run {timeout_s} {
    scan_cmd $::OP_START 0 0 {}
    set t0 [clock milliseconds]
    while {1} {
        set r [status]
        if {[dict get $r done] && ![dict get $r busy]} break
        if {[clock milliseconds] - $t0 > 1000 * $timeout_s} {
            error "timeout: el programa no llegó a HALT en $timeout_s s"
        }
    }
    return "[dict get $r cycles] [dict get $r error]"
}

proc cmd_reset {} {
    scan_cmd $::OP_RESET 0 0 {}
    flush_cmds
}

set cin stdin
set cout stdout
proc reply {msg} {
    puts $::cout "@@ $msg"
    flush $::cout
}

# ======================================================================= main
set mode [lindex $argv 0]
if {$mode eq ""} { set mode ping }

if {$mode eq "server" && [info exists ::env(VPROC_JTAG_CMD)]} {
    set cin  [open $::env(VPROC_JTAG_CMD) r]
    set cout [open $::env(VPROC_JTAG_REPLY) w]
}

if {[catch {set where [jt_open]; sync} err]} {
    reply "ERR $err"
    jt_close
    exit 1
}

if {$mode eq "ping"} {
    set r [status]
    puts "Puente VPROC encontrado en $where"
    puts "  seq=[dict get $r seq] busy=[dict get $r busy] done=[dict get $r done] error=[dict get $r error] ciclos=[dict get $r cycles]"
    jt_close
    exit 0
}

reply "READY $where"
while {[gets $cin line] >= 0} {
    set args [split [string trim $line]]
    set cmd [string toupper [lindex $args 0]]
    if {$cmd eq ""} continue
    if {$cmd eq "QUIT"} break
    set rc [catch {
        switch -- $cmd {
            PING  { set r [status]
                    set res "[dict get $r seq] [dict get $r busy] [dict get $r done] [dict get $r error] [dict get $r cycles]" }
            PROG  { set res [cmd_prog [lindex $args 1]] }
            WRITE { set res [cmd_write [lindex $args 1] [lindex $args 2]] }
            READ  { set res [cmd_read [lindex $args 1] [lindex $args 2] [lindex $args 3]] }
            RUN   { set res [cmd_run [lindex $args 1]] }
            RESET { cmd_reset; set res "" }
            default { error "comando desconocido: $cmd" }
        }
    } err]
    if {$rc} {
        reply "ERR $err"
        # Vuelve a sincronizar para que el siguiente comando arranque limpio
        catch {sync}
    } else {
        reply "OK $res"
    }
}
catch {reply "BYE"}
jt_close
exit 0
