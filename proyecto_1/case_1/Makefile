# =================================================
# Configuration: Block Matrix Multiplication —
# Multithreading Execution Model Framework (CE4302)
#
# =================================================

CC       := gcc
# -I. permite que engines/*.c hagan #include "matrix.h" encontrando el
# header un nivel arriba, en la raiz de case_1.
CFLAGS   := -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE -I.
LDFLAGS  := -pthread

ENGINES_DIR := engines
BUILD       := build
RESULTS     := results
PERF_RESULTS := perf_results

COMMON_SRC := matrix.c
COMMON_HDR := matrix.h
COMMON_OBJ := $(BUILD)/matrix.o

PROGRAMS := sequential fineGrained coarseGrained smt cmp
BINARIES := $(addprefix $(BUILD)/,$(PROGRAMS))

# ------- parametros de experimento (sobreescribibles por linea de comando) ----
N            ?= 512
BLOCK_SIZE   ?= 64
THREADS      ?= 4
VTHREADS     ?= 4
SMT_CORES    ?= 2
SMT_FACTOR   ?= 2
ITER         ?= 200
SEED         ?= 42

# ------- listas para el barrido (sweep) de numero de hilos/cores --------
# THREAD_LIST  : valores de THREADS usados por coarseGrained y cmp.
# VTHREAD_LIST : valores de VTHREADS usados por fineGrained.
# SMT_COMBOS   : pares "nucleos_fisicos:factor_smt" usados por smt.
THREAD_LIST  ?= 1 2 3 4
VTHREAD_LIST ?= 1 2 4 8 16
SMT_COMBOS   ?= 1:1 2:1 4:1 4:2 4:3 4:4

# ------- parametros para el perfilado con perf (M4) ----------------------
# Independientes de N/BLOCK_SIZE/SEED de arriba para no interferir con el
# sweep estadistico.
PERF_N          ?= $(N)
PERF_BLOCK_SIZE ?= $(BLOCK_SIZE)
PERF_SEED       ?= $(SEED)
# `perf stat` en una sola corrida es una muestra de n=1 y varia mucho de
# una ejecucion a otra (ruido del sistema, frecuencia dinamica, etc.).
# -r repite el binario PERF_REPEAT veces y perf reporta media +/- desv.
# estandar automaticamente, igual que el sweep estadistico de 200 iter.
PERF_REPEAT     ?= 10
# Numero de cores fisicos a usar en cmp/smt durante el perfilado (4 en un
# i7-1165G7: 4 cores fisicos / 8 hilos logicos con Hyper-Threading).
PERF_CORES      ?= 4
# Combos physical_cores:smt_factor a perfilar con perf stat: sin HT, HT
# exacto (satura los hilos logicos reales) y sobre-suscrito mas alla del
# hardware.
PERF_SMT_COMBOS ?= $(PERF_CORES):1 $(PERF_CORES):2 $(PERF_CORES):4
PERF_STAT_EVENTS ?= task-clock,cycles,instructions,cache-references,cache-misses,context-switches,cpu-migrations,stalled-cycles-frontend,stalled-cycles-backend


# =================================================
# Targets
# =================================================

.PHONY: all help build results-dir perf-results-dir \
        run-sequential run-fine run-coarse run-smt run-cmp \
        experiments sweep sweep-quick \
        perf-topology perf-sequential perf-cmp perf-smt perf-smt-percpu perf-threads-cpu perf-record-smt perf-all \
        clean

all: help

help: ## Show available targets
	@echo "Available targets:"
	@grep -E '^[a-zA-Z0-9_%./-]+:.*## ' $(MAKEFILE_LIST) | \
	awk 'BEGIN {FS = ":.*## "}; {printf "  %-20s %s\n", $$1, $$2}'
	@echo ""
	@echo "Override experiment parameters, e.g.:"
	@echo "  make run-cmp N=1024 BLOCK_SIZE=128 THREADS=8"
	@echo "  make experiments N=1024 BLOCK_SIZE=128 THREADS=8 ITER=200"
	@echo "  make sweep N=1024 BLOCK_SIZE=128 ITER=200"
	@echo "  make perf-all PERF_N=1024 PERF_BLOCK_SIZE=128 PERF_CORES=4"

build: $(BINARIES) ## Compile the 5 programs (sequential + 4 execution models)

results-dir:
	@mkdir -p $(RESULTS)

perf-results-dir:
	@mkdir -p $(PERF_RESULTS)

# --- corridas individuales (una fila CSV por stdout) ----------------------
run-sequential: $(BUILD)/sequential ## Run the sequential baseline once
	$(BUILD)/sequential $(N) $(BLOCK_SIZE) $(SEED)

run-fine: $(BUILD)/fineGrained ## Run fine-grained model once
	$(BUILD)/fineGrained $(N) $(BLOCK_SIZE) $(VTHREADS) $(SEED)

run-coarse: $(BUILD)/coarseGrained ## Run coarse-grained model once
	$(BUILD)/coarseGrained $(N) $(BLOCK_SIZE) $(THREADS) $(SEED)

run-smt: $(BUILD)/smt ## Run SMT (software approximation) once
	$(BUILD)/smt $(N) $(BLOCK_SIZE) $(SMT_CORES) $(SMT_FACTOR) $(SEED)

run-cmp: $(BUILD)/cmp ## Run CMP model once
	$(BUILD)/cmp $(N) $(BLOCK_SIZE) $(THREADS) $(SEED)

# --- experimentos por lote: >= ITER repeticiones por configuracion --------
# Cada programa imprime un encabezado CSV en cada corrida; se descarta el
# encabezado repetido con `tail -n +2` para que results/*.csv termine con
# una sola linea de encabezado.
experiments: build results-dir ## Run ITER (default 200) repetitions per model, write results/*.csv
	@echo "Corriendo $(ITER) repeticiones por modelo (N=$(N) BLOCK_SIZE=$(BLOCK_SIZE))..."
	@( $(BUILD)/sequential $(N) $(BLOCK_SIZE) $(SEED); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/sequential $(N) $(BLOCK_SIZE) $$(($(SEED)+i)) | tail -n +2; \
	   done ) > $(RESULTS)/sequential.csv
	@( $(BUILD)/fineGrained $(N) $(BLOCK_SIZE) $(VTHREADS) $(SEED); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/fineGrained $(N) $(BLOCK_SIZE) $(VTHREADS) $$(($(SEED)+i)) | tail -n +2; \
	   done ) > $(RESULTS)/fine_grained.csv
	@( $(BUILD)/coarseGrained $(N) $(BLOCK_SIZE) $(THREADS) $(SEED); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/coarseGrained $(N) $(BLOCK_SIZE) $(THREADS) $$(($(SEED)+i)) | tail -n +2; \
	   done ) > $(RESULTS)/coarse_grained.csv
	@( $(BUILD)/smt $(N) $(BLOCK_SIZE) $(SMT_CORES) $(SMT_FACTOR) $(SEED); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/smt $(N) $(BLOCK_SIZE) $(SMT_CORES) $(SMT_FACTOR) $$(($(SEED)+i)) | tail -n +2; \
	   done ) > $(RESULTS)/smt.csv
	@( $(BUILD)/cmp $(N) $(BLOCK_SIZE) $(THREADS) $(SEED); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/cmp $(N) $(BLOCK_SIZE) $(THREADS) $$(($(SEED)+i)) | tail -n +2; \
	   done ) > $(RESULTS)/cmp.csv
	@echo "Listo. Resultados en $(RESULTS)/*.csv"

# --- barrido (sweep): ITER repeticiones POR CADA valor de hilos/cores -----
# A diferencia de `experiments` (que fija un solo P), este target recorre
# THREAD_LIST / VTHREAD_LIST / SMT_COMBOS para poder calcular speedup,
# eficiencia y escalabilidad en funcion del numero de hilos/cores (inciso
# "d" del enunciado). Cada CSV de salida termina con un solo encabezado,
# sin importar cuantas configuraciones de P se hayan corrido.
sweep: build results-dir ## Run ITER repetitions for EACH thread-count in *_LIST/SMT_COMBOS (scalability data)
	@echo "=== Barrido secuencial (baseline, no varia P) ==="
	@( $(BUILD)/sequential $(N) $(BLOCK_SIZE) $$(($(SEED)+1)); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/sequential $(N) $(BLOCK_SIZE) $$(($(SEED)+i)) | tail -n +2; \
	   done ) > $(RESULTS)/sequential.csv
	@echo "=== Barrido fine-grained (VTHREAD_LIST=$(VTHREAD_LIST)) ==="
	@rm -f $(RESULTS)/fine_grained.csv
	@first=1; \
	for vt in $(VTHREAD_LIST); do \
	  echo "  -> vthreads=$$vt"; \
	  for i in $$(seq 1 $(ITER)); do \
	    if [ $$first -eq 1 ]; then \
	      $(BUILD)/fineGrained $(N) $(BLOCK_SIZE) $$vt $$(($(SEED)+i)) >> $(RESULTS)/fine_grained.csv; \
	      first=0; \
	    else \
	      $(BUILD)/fineGrained $(N) $(BLOCK_SIZE) $$vt $$(($(SEED)+i)) | tail -n +2 >> $(RESULTS)/fine_grained.csv; \
	    fi; \
	  done; \
	done
	@echo "=== Barrido coarse-grained (THREAD_LIST=$(THREAD_LIST)) ==="
	@rm -f $(RESULTS)/coarse_grained.csv
	@first=1; \
	for t in $(THREAD_LIST); do \
	  echo "  -> threads=$$t"; \
	  for i in $$(seq 1 $(ITER)); do \
	    if [ $$first -eq 1 ]; then \
	      $(BUILD)/coarseGrained $(N) $(BLOCK_SIZE) $$t $$(($(SEED)+i)) >> $(RESULTS)/coarse_grained.csv; \
	      first=0; \
	    else \
	      $(BUILD)/coarseGrained $(N) $(BLOCK_SIZE) $$t $$(($(SEED)+i)) | tail -n +2 >> $(RESULTS)/coarse_grained.csv; \
	    fi; \
	  done; \
	done
	@echo "=== Barrido SMT (SMT_COMBOS=$(SMT_COMBOS)) ==="
	@rm -f $(RESULTS)/smt.csv
	@first=1; \
	for combo in $(SMT_COMBOS); do \
	  pc=$$(echo $$combo | cut -d: -f1); \
	  sf=$$(echo $$combo | cut -d: -f2); \
	  echo "  -> physical_cores=$$pc smt_factor=$$sf"; \
	  for i in $$(seq 1 $(ITER)); do \
	    if [ $$first -eq 1 ]; then \
	      $(BUILD)/smt $(N) $(BLOCK_SIZE) $$pc $$sf $$(($(SEED)+i)) >> $(RESULTS)/smt.csv; \
	      first=0; \
	    else \
	      $(BUILD)/smt $(N) $(BLOCK_SIZE) $$pc $$sf $$(($(SEED)+i)) | tail -n +2 >> $(RESULTS)/smt.csv; \
	    fi; \
	  done; \
	done
	@echo "=== Barrido CMP (THREAD_LIST=$(THREAD_LIST)) ==="
	@rm -f $(RESULTS)/cmp.csv
	@first=1; \
	for t in $(THREAD_LIST); do \
	  echo "  -> threads=$$t"; \
	  for i in $$(seq 1 $(ITER)); do \
	    if [ $$first -eq 1 ]; then \
	      $(BUILD)/cmp $(N) $(BLOCK_SIZE) $$t $$(($(SEED)+i)) >> $(RESULTS)/cmp.csv; \
	      first=0; \
	    else \
	      $(BUILD)/cmp $(N) $(BLOCK_SIZE) $$t $$(($(SEED)+i)) | tail -n +2 >> $(RESULTS)/cmp.csv; \
	    fi; \
	  done; \
	done
	@echo "Listo. Resultados (multi-P) en $(RESULTS)/*.csv"

sweep-quick: ## Fast sanity check of the sweep pipeline (small N, ITER=5, short lists)
	@$(MAKE) sweep \
	  N=128 BLOCK_SIZE=32 ITER=5 \
	  THREAD_LIST="1 2 4" \
	  VTHREAD_LIST="1 2 4" \
	  SMT_COMBOS="1:1 2:1 2:2"

# =================================================
# Perfilado con perf (M4): foco especial en SMT
# =================================================
# Estos targets corren UNA sola vez cada binario (no son estadisticos como
# `sweep`/`experiments`): el objetivo es capturar contadores de hardware
# (ciclos, instrucciones, IPC, cache, stalls) y guardarlos como evidencia
# en perf_results/, no medir tiempo con repeticiones.
#
# Requiere `perf` instalado y, en Linux, perf_event_paranoid <= 1:
#   sudo sysctl kernel.perf_event_paranoid=1

perf-topology: perf-results-dir ## Dump CPU topology (cores/threads) as profiling evidence
	lscpu > $(PERF_RESULTS)/00-lscpu.txt
	@echo "-> $(PERF_RESULTS)/00-lscpu.txt"

perf-sequential: build perf-results-dir ## perf stat (PERF_REPEAT runs, mean +/- stddev) on the sequential baseline
	perf stat -r $(PERF_REPEAT) -e $(PERF_STAT_EVENTS) \
	  $(BUILD)/sequential $(PERF_N) $(PERF_BLOCK_SIZE) $(PERF_SEED) \
	  2>&1 | tee $(PERF_RESULTS)/01-sequential.txt

perf-cmp: build perf-results-dir ## perf stat (PERF_REPEAT runs) on CMP at PERF_CORES (real parallelism reference)
	perf stat -r $(PERF_REPEAT) -e $(PERF_STAT_EVENTS) \
	  $(BUILD)/cmp $(PERF_N) $(PERF_BLOCK_SIZE) $(PERF_CORES) $(PERF_SEED) \
	  2>&1 | tee $(PERF_RESULTS)/02-cmp-p$(PERF_CORES).txt

perf-smt: build perf-results-dir ## perf stat (PERF_REPEAT runs each) on SMT for each combo in PERF_SMT_COMBOS (no-HT / HT-exact / oversubscribed)
	@for combo in $(PERF_SMT_COMBOS); do \
	  pc=$$(echo $$combo | cut -d: -f1); \
	  sf=$$(echo $$combo | cut -d: -f2); \
	  echo "=== perf stat smt physical_cores=$$pc smt_factor=$$sf (x$(PERF_REPEAT)) ==="; \
	  perf stat -r $(PERF_REPEAT) -e $(PERF_STAT_EVENTS) \
	    $(BUILD)/smt $(PERF_N) $(PERF_BLOCK_SIZE) $$pc $$sf $(PERF_SEED) \
	    2>&1 | tee $(PERF_RESULTS)/03-smt-$${pc}x$${sf}.txt; \
	done

# NOTA (historial de intentos fallidos, se deja como evidencia del
# proceso de depuracion):
#   1) `perf stat -a -A ...`           -> mide TODO el sistema por CPU,
#      no es especifico de nuestro proceso. Ademas requiere
#      perf_event_paranoid <= 0.
#   2) `perf stat --per-thread ...`    -> rechazado por este perf:
#      "The --per-thread option is only available when monitoring via
#      -p -t -a options or only --per-thread."
#   3) `perf stat -a --per-thread ...` -> corre sin error, pero NUNCA
#      aparece ninguna fila con comm "smt", ni con PERF_N chico (~90ms)
#      ni con una corrida de ~3.6s (PERF_PERCPU_N=2048). Causa real: en
#      modo system-wide (-a), perf construye su tabla de hilos a partir
#      de lo que ya existe en el sistema (via /proc + eventos de
#      fork/exec) en el momento en que ARRANCA; el hijo que el propio
#      `perf` lanza (build/smt) nace despues de ese arranque y, en esta
#      combinacion concreta, sus pthreads nunca quedan registrados en el
#      desglose por hilo. No es un problema de duracion del programa,
#      es un problema de que `-a --per-thread` no esta pensado para
#      perfilar por hilo al proceso hijo que el mismo comando lanza.
#
# SOLUCION: dejar de usar `-a` (system-wide) y en su lugar ADJUNTARSE
# directamente al PID de `smt` una vez que ya esta vivo (`perf stat -p
# PID --per-thread`), que es justo el modo que el mensaje de error de (2)
# decia que si es valido. Para esto:
#   - se lanza `smt` en background y se captura su PID ($$!),
#   - se espera PERF_ATTACH_DELAY segundos (tiempo de sobra para que ya
#     haya hecho los pthread_create de sus hilos trabajadores),
#   - se adjunta `perf stat -p PID --per-thread` tambien en background,
#   - se espera a que `smt` termine, se le manda SIGINT a `perf` (como
#     un Ctrl+C) para que imprima sus contadores y termine.
#
# PERF_PERCPU_N: usa un N mas grande que PERF_N SOLO para este target y
# para `perf-threads-cpu`, para que la corrida dure varios segundos y dé
# tiempo de attachar perf y de tomar varias muestras con `ps`. No cambia
# PERF_N de los demas targets (01-03), que si queremos representativo
# del experimento real.
PERF_PERCPU_N ?= 2048
# Tiempo (segundos) que se espera despues de lanzar `smt` en background y
# ANTES de adjuntar `perf stat -p PID`, para darle tiempo a que el hilo
# principal haga los pthread_create() de sus hilos trabajadores. 0.1s es
# de sobra: la creacion de hilos ocurre en los primeros ms del programa.
PERF_ATTACH_DELAY ?= 0.1

perf-smt-percpu: build perf-results-dir ## Per-thread perf stat of SMT at PERF_CORES x2 via `perf stat -p PID` (se adjunta al proceso ya vivo, evita el modo -a system-wide)
	@rm -f $(PERF_RESULTS)/04-smt-percpu-output.csv $(PERF_RESULTS)/04-smt-per-thread.txt
	@( $(BUILD)/smt $(PERF_PERCPU_N) $(PERF_BLOCK_SIZE) $(PERF_CORES) 2 $(PERF_SEED) \
	     > $(PERF_RESULTS)/04-smt-percpu-output.csv & \
	   SMT_PID=$$!; \
	   echo "smt PID=$$SMT_PID, esperando $(PERF_ATTACH_DELAY)s a que nazcan los hilos..."; \
	   sleep $(PERF_ATTACH_DELAY); \
	   perf stat -p $$SMT_PID --per-thread -e task-clock,cycles,instructions \
	     > $(PERF_RESULTS)/04-smt-per-thread.txt 2>&1 & \
	   PERF_PID=$$!; \
	   wait $$SMT_PID; \
	   sleep 0.2; \
	   kill -INT $$PERF_PID 2>/dev/null; \
	   wait $$PERF_PID 2>/dev/null; \
	   true )
	@cat $(PERF_RESULTS)/04-smt-per-thread.txt
	@echo "-> $(PERF_RESULTS)/04-smt-per-thread.txt (una fila por TID real de smt, con task-clock/cycles/instructions)"
	@echo "-> salida numerica (CSV del propio programa): $(PERF_RESULTS)/04-smt-percpu-output.csv"

perf-threads-cpu: build perf-results-dir ## Muestrea con `ps -L` cada 0.1s que CPU logica (psr) usa cada hilo de smt durante toda la corrida (evidencia directa y confiable para "uso de CPU por hilo logico")
	@rm -f $(PERF_RESULTS)/04b-smt-output.csv
	@echo "timestamp,pid,tid,psr,pcpu,comm" > $(PERF_RESULTS)/04b-smt-thread-cpu-samples.csv
	@( $(BUILD)/smt $(PERF_PERCPU_N) $(PERF_BLOCK_SIZE) $(PERF_CORES) 2 $(PERF_SEED) \
	     > $(PERF_RESULTS)/04b-smt-output.csv & \
	   SMT_PID=$$!; \
	   echo "smt PID=$$SMT_PID, muestreando ps -L cada 0.1s hasta que termine..."; \
	   while kill -0 $$SMT_PID 2>/dev/null; do \
	     ts=$$(date +%s.%N); \
	     ps -L -o tid,psr,pcpu,comm -p $$SMT_PID --no-headers 2>/dev/null | \
	       awk -v ts=$$ts -v pid=$$SMT_PID '{printf "%s,%s,%s,%s,%s,%s\n", ts, pid, $$1, $$2, $$3, $$4}' \
	       >> $(PERF_RESULTS)/04b-smt-thread-cpu-samples.csv; \
	     sleep 0.1; \
	   done )
	@echo "-> $(PERF_RESULTS)/04b-smt-thread-cpu-samples.csv (columna psr = CPU logica real usada por cada hilo, una muestra cada 0.1s)"
	@echo "-> resumen (hilos distintos vs CPUs logicas distintas observadas):"
	@tail -n +2 $(PERF_RESULTS)/04b-smt-thread-cpu-samples.csv | awk -F, '{print $$3","$$4}' | sort -u | \
	  awk -F, '{tids[$$1]=1; psrs[$$2]=1} END {print "   hilos (TID) distintos: " length(tids); print "   CPUs logicas (PSR) distintas: " length(psrs)}'

perf-record-smt: build perf-results-dir ## perf record + report of SMT at PERF_CORES x2 (hotspots inside mat_block_full)
	perf record -g -o $(PERF_RESULTS)/smt-$(PERF_CORES)x2.data -- \
	  $(BUILD)/smt $(PERF_N) $(PERF_BLOCK_SIZE) $(PERF_CORES) 2 $(PERF_SEED)
	perf report -i $(PERF_RESULTS)/smt-$(PERF_CORES)x2.data --stdio \
	  > $(PERF_RESULTS)/05-smt-$(PERF_CORES)x2-report.txt
	@echo "-> $(PERF_RESULTS)/05-smt-$(PERF_CORES)x2-report.txt"

perf-all: perf-topology perf-sequential perf-cmp perf-smt perf-smt-percpu perf-threads-cpu perf-record-smt ## Run the full M4 profiling pass in one go
	@echo "Listo. Evidencia de perfilado en $(PERF_RESULTS)/"

clean: ## Remove build artifacts and results
	@rm -rf $(BUILD) $(RESULTS)


# =================================================
# Build rules
# =================================================

$(COMMON_OBJ): $(COMMON_SRC) $(COMMON_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $(COMMON_SRC) -o $@

$(BUILD)/sequential: $(ENGINES_DIR)/sequential.c $(COMMON_OBJ) $(COMMON_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< $(COMMON_OBJ) -o $@ $(LDFLAGS)

$(BUILD)/fineGrained: $(ENGINES_DIR)/fineGrained.c $(COMMON_OBJ) $(COMMON_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< $(COMMON_OBJ) -o $@ $(LDFLAGS)

$(BUILD)/coarseGrained: $(ENGINES_DIR)/coarseGrained.c $(COMMON_OBJ) $(COMMON_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< $(COMMON_OBJ) -o $@ $(LDFLAGS)

$(BUILD)/smt: $(ENGINES_DIR)/smt.c $(COMMON_OBJ) $(COMMON_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< $(COMMON_OBJ) -o $@ $(LDFLAGS)

$(BUILD)/cmp: $(ENGINES_DIR)/cmp.c $(COMMON_OBJ) $(COMMON_HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $< $(COMMON_OBJ) -o $@ $(LDFLAGS)