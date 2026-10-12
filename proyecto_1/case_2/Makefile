# =================================================
# case_2 — N-body gravitacional (CE4302, Proyecto 1)
#
# Sigue la misma convencion que case_1: este Makefile expone los
# targets "build", "experiments" y "clean" que el Makefile general de
# proyecto_1/ invoca via `make -C case_2 <target>`. Ademas agrega
# targets cortos para compilar y correr cada programa a mano mientras
# se prueba, sin tener que escribir el comando de gcc completo cada vez.
# =================================================

CC      := gcc
# -I.       : los .c de este mismo directorio (Main.c, Bench*.c) ya ven
#             NBody.h sin ayuda, pero -I. tambien cubre tests/*.c, que
#             hacen #include "NBody.h" estando un nivel mas adentro.
# -Iengines : BenchCmp.c, BenchSmt.c, BenchCgmt.c, BenchFgmt.c y
#             tests/TestCmp.c, TestCgmt.c, TestFgmt.c hacen
#             #include "Cmp.h" / "Smt.h" / "Cgmt.h" / "Fgmt.h", que
#             viven en engines/.
CFLAGS  := -O2 -Wall -march=native -I. -Iengines
LDFLAGS := -lm -lpthread

ENGINES := engines
TESTS   := tests

# ------- parametros de experimento (sobreescribibles por linea de comando) ----
N        ?= 1024
STEPS    ?= 200
THREADS  ?= 4
CHUNK    ?= 32
FIBERS   ?= 8
QUANTUM  ?= 64
SEED     ?= 12345

BINARIES := nbody bench benchcmp benchsmt benchcgmt benchfgmt
TEST_BINARIES := test_forces test_cmp test_cgmt test_fgmt

.PHONY: all help build test clean experiments \
        run-nbody run-bench run-cmp run-smt run-cgmt run-fgmt \
        test-forces test-cmp test-cgmt test-fgmt \
        campaign-cmp campaign-smt campaign-cgmt campaign-fgmt analyze \
        perf-smt perf-smt-thread

all: help

help: ## Show available targets
	@echo "Available targets:"
	@grep -E '^[a-zA-Z0-9_%./-]+:.*## ' $(MAKEFILE_LIST) | \
	awk 'BEGIN {FS = ":.*## "}; {printf "  %-20s %s\n", $$1, $$2}'
	@echo ""
	@echo "Override parametros, por ejemplo:"
	@echo "  make run-cmp N=4096 THREADS=8"
	@echo "  make run-fgmt N=2048 FIBERS=16 QUANTUM=8"

build: $(BINARIES) $(TEST_BINARIES) ## Compile every program and test

# --- correctitud: compara cada modelo contra el secuencial -----------------

test: test-forces test-cmp test-cgmt test-fgmt ## Build and run all 4 correctness tests in order

test-forces: test_forces ## Run the sequential force sanity checks
	./test_forces

test-cmp: test_cmp ## Compare CMP acceleration against sequential
	./test_cmp

test-cgmt: test_cgmt ## Compare CGMT acceleration against sequential
	./test_cgmt

test-fgmt: test_fgmt ## Compare FGMT acceleration against sequential
	./test_fgmt

# --- correr un benchmark una vez, con los parametros de arriba -------------

run-nbody: nbody ## Run the 1000-step energy/momentum sanity program
	./nbody

run-bench: bench ## Time the sequential baseline (N, STEPS, SEED)
	./bench $(N) $(STEPS) $(SEED)

run-cmp: benchcmp ## Time the CMP model (N, STEPS, THREADS)
	./benchcmp $(N) $(STEPS) $(THREADS)

run-smt: benchsmt ## Time the SMT model (N, STEPS, THREADS)
	./benchsmt $(N) $(STEPS) $(THREADS)

run-cgmt: benchcgmt ## Time the CGMT model (N, STEPS, THREADS, CHUNK)
	./benchcgmt $(N) $(STEPS) $(THREADS) $(CHUNK)

run-fgmt: benchfgmt ## Time the FGMT model (N, STEPS, FIBERS, QUANTUM)
	./benchfgmt $(N) $(STEPS) $(FIBERS) $(QUANTUM)

# --- campanas completas (200 repeticiones, scripts ya calibrados) ----------

campaign-cmp: benchcmp ## Run the full CMP measurement campaign -> final_cmp.csv
	chmod +x campaign_cmp.sh
	./campaign_cmp.sh

campaign-smt: benchsmt ## Run the full SMT measurement campaign -> final_smt.csv
	chmod +x campaign_smt.sh
	./campaign_smt.sh

campaign-cgmt: benchcgmt ## Run the full CGMT measurement campaign -> final_cgmt.csv
	chmod +x campaign_cgmt.sh
	./campaign_cgmt.sh

campaign-fgmt: benchfgmt ## Run the full FGMT measurement campaign -> final_fgmt.csv
	chmod +x campaign_fgmt.sh
	./campaign_fgmt.sh

experiments: campaign-cmp campaign-smt campaign-cgmt campaign-fgmt ## Run all 4 campaigns (what proyecto_1/Makefile calls)

analyze: ## Post-process the 4 final_*.csv into summary.csv and the plots
	python3 analyze_campaign.py

# --- perfilado con perf (requisito del enunciado: comportamiento bajo SMT) -

perf-smt: benchsmt ## Perf stat on benchsmt — rename output after each SMT BIOS toggle
	perf stat -e cycles,instructions,context-switches,cpu-migrations \
	    ./benchsmt $(N) $(STEPS) $(THREADS) 2> perf_smt.txt
	@echo "Guardado en perf_smt.txt -- renombralo a perf_smt_on.txt o"
	@echo "perf_smt_off.txt segun el estado de SMT en el BIOS antes de"
	@echo "correrlo otra vez con el otro estado."

perf-smt-thread: benchsmt ## Perf stat per-thread on benchsmt (uso de CPU por hilo logico)
	perf stat --per-thread \
	    ./benchsmt $(N) $(STEPS) $(THREADS) 2> perf_smt_thread.txt
	@echo "Guardado en perf_smt_thread.txt"

clean: ## Remove every compiled binary (keeps .c/.h/.sh/.csv sources)
	rm -f $(BINARIES) $(TEST_BINARIES)

# =================================================
# Build rules
# =================================================

nbody: Main.c Nbody.c NBody.h
	$(CC) $(CFLAGS) Main.c Nbody.c -o $@ $(LDFLAGS)

bench: Bench.c Nbody.c NBody.h
	$(CC) $(CFLAGS) Bench.c Nbody.c -o $@ $(LDFLAGS)

benchcmp: BenchCmp.c Nbody.c $(ENGINES)/Cmp.c NBody.h $(ENGINES)/Cmp.h
	$(CC) $(CFLAGS) BenchCmp.c Nbody.c $(ENGINES)/Cmp.c -o $@ $(LDFLAGS)

benchsmt: BenchSmt.c Nbody.c $(ENGINES)/Cmp.c $(ENGINES)/Smt.c NBody.h $(ENGINES)/Smt.h
	$(CC) $(CFLAGS) BenchSmt.c Nbody.c $(ENGINES)/Cmp.c $(ENGINES)/Smt.c -o $@ $(LDFLAGS)

benchcgmt: BenchCgmt.c Nbody.c $(ENGINES)/Cgmt.c NBody.h $(ENGINES)/Cgmt.h
	$(CC) $(CFLAGS) BenchCgmt.c Nbody.c $(ENGINES)/Cgmt.c -o $@ $(LDFLAGS)

benchfgmt: BenchFgmt.c Nbody.c $(ENGINES)/Fgmt.c NBody.h $(ENGINES)/Fgmt.h
	$(CC) $(CFLAGS) BenchFgmt.c Nbody.c $(ENGINES)/Fgmt.c -o $@ $(LDFLAGS)

test_forces: $(TESTS)/TestForces.c Nbody.c NBody.h
	$(CC) $(CFLAGS) $(TESTS)/TestForces.c Nbody.c -o $@ $(LDFLAGS)

test_cmp: $(TESTS)/TestCmp.c Nbody.c $(ENGINES)/Cmp.c NBody.h $(ENGINES)/Cmp.h
	$(CC) $(CFLAGS) $(TESTS)/TestCmp.c Nbody.c $(ENGINES)/Cmp.c -o $@ $(LDFLAGS)

test_cgmt: $(TESTS)/TestCgmt.c Nbody.c $(ENGINES)/Cgmt.c NBody.h $(ENGINES)/Cgmt.h
	$(CC) $(CFLAGS) $(TESTS)/TestCgmt.c Nbody.c $(ENGINES)/Cgmt.c -o $@ $(LDFLAGS)

test_fgmt: $(TESTS)/TestFgmt.c Nbody.c $(ENGINES)/Fgmt.c NBody.h $(ENGINES)/Fgmt.h
	$(CC) $(CFLAGS) $(TESTS)/TestFgmt.c Nbody.c $(ENGINES)/Fgmt.c -o $@ $(LDFLAGS)