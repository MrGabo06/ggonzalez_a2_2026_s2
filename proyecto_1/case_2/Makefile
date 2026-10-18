# =================================================
# case_2 — N-body gravitacional (CE4302, Proyecto 1)
#
# Layout (sigue la convencion de case_1):
#   engines/   motores de cada modelo de ejecucion (sin cambios)
#   tests/     pruebas de correctitud (sin cambios)
#   build/     binarios compilados (gitignored, se regeneran con `make build`)
#   scripts/   campanas de medicion y post-procesamiento
#   results/
#     raw/       CSVs crudos: final_cmp_{on,off}.csv y final_smt_{on,off}.csv
#                (dos sesiones de SMT), final_cgmt.csv, final_fgmt.csv,
#                campanas piloto, summary*.csv
#     figures/   PNGs generados por los scripts de analisis
#     perf/      capturas de `perf stat` y muestreo de CPU logica
# =================================================

CC      := gcc
CFLAGS  := -O2 -Wall -march=native -I. -Iengines
LDFLAGS := -lm -lpthread

ENGINES := engines
TESTS   := tests
BUILD   := build
SCRIPTS := scripts
RESULTS_RAW     := results/raw
RESULTS_FIGURES := results/figures
RESULTS_PERF    := results/perf

# ------- parametros de experimento (sobreescribibles por linea de comando) ----
N        ?= 1024
STEPS    ?= 200
THREADS  ?= 4
CHUNK    ?= 32
FIBERS   ?= 8
QUANTUM  ?= 64
SEED     ?= 12345

# Estado de SMT con el que se ETIQUETAN las salidas de campanas y perf
# (on|off). No cambia SMT en el sistema: hay que apagarlo/encenderlo antes
# y luego correr con el SMT= que corresponda, p. ej. `make campaign-cmp SMT=off`.
SMT      ?= on
# N usado en las graficas/tablas del paper y en el perfilado.
PAPER_N  ?= 4096
# Hilos del perfilado de SMT (scripts/perf_smt_thread.sh, sample_cpu_affinity.sh).
PERF_THREADS ?= 8
# Modo defensa (`make sweep-quick`): una version corta de las campanas.
QUICK_N     ?= 4096
QUICK_STEPS ?= 8
QUICK_REPS  ?= 30
# Modelos del modo defensa (subconjunto de: cmp smt cgmt fgmt). El secuencial
# siempre corre. Ej.: `make sweep-quick MODELS=cgmt` o MODELS="cmp smt".
MODELS      ?= cmp smt cgmt fgmt
# Repetir solo algunos hilos de la campana de SMT (p. ej. SMT_WORKERS="8 16"):
# escribe results/raw/final_smt_<SMT>_rerun.csv sin tocar el archivo completo.
SMT_WORKERS ?=
# APPLY=1 hace que `make merge-smt-rerun` escriba el reemplazo (sin APPLY solo reporta).
APPLY       ?=

PROGRAMS     := nbody bench benchcmp benchsmt benchcgmt benchfgmt
TEST_PROGRAMS := test_forces test_cmp test_cgmt test_fgmt
BINARIES      := $(addprefix $(BUILD)/,$(PROGRAMS))
TEST_BINARIES := $(addprefix $(BUILD)/,$(TEST_PROGRAMS))

.PHONY: all help build test clean experiments sweep sweep-quick \
        run-nbody run-bench run-cmp run-smt run-cgmt run-fgmt \
        test-forces test-cmp test-cgmt test-fgmt \
        campaign-cmp campaign-smt campaign-cgmt campaign-fgmt \
        rerun-smt merge-smt-rerun \
        analyze analyze-smt plot-affinity paper-table analyze-all \
        perf-smt perf-smt-thread sample-affinity

all: help

help: ## Show available targets
	@echo "Available targets:"
	@grep -E '^[a-zA-Z0-9_%./-]+:.*## ' $(MAKEFILE_LIST) | \
	awk 'BEGIN {FS = ":.*## "}; {printf "  %-20s %s\n", $$1, $$2}'
	@echo ""
	@echo "Override parametros, por ejemplo:"
	@echo "  make run-cmp N=4096 THREADS=8"
	@echo "  make run-fgmt N=2048 FIBERS=16 QUANTUM=8"

build: $(BINARIES) $(TEST_BINARIES) ## Compile every program and test into build/

# --- correctitud: compara cada modelo contra el secuencial -----------------

test: test-forces test-cmp test-cgmt test-fgmt ## Build and run all 4 correctness tests in order

test-forces: $(BUILD)/test_forces ## Run the sequential force sanity checks
	$(BUILD)/test_forces

test-cmp: $(BUILD)/test_cmp ## Compare CMP acceleration against sequential
	$(BUILD)/test_cmp

test-cgmt: $(BUILD)/test_cgmt ## Compare CGMT acceleration against sequential
	$(BUILD)/test_cgmt

test-fgmt: $(BUILD)/test_fgmt ## Compare FGMT acceleration against sequential
	$(BUILD)/test_fgmt

# --- correr un benchmark una vez, con los parametros de arriba -------------

run-nbody: $(BUILD)/nbody ## Run the 1000-step energy/momentum sanity program
	$(BUILD)/nbody

run-bench: $(BUILD)/bench ## Time the sequential baseline (N, STEPS, SEED)
	$(BUILD)/bench $(N) $(STEPS) $(SEED)

run-cmp: $(BUILD)/benchcmp ## Time the CMP model (N, STEPS, THREADS)
	$(BUILD)/benchcmp $(N) $(STEPS) $(THREADS)

run-smt: $(BUILD)/benchsmt ## Time the SMT model (N, STEPS, THREADS)
	$(BUILD)/benchsmt $(N) $(STEPS) $(THREADS)

run-cgmt: $(BUILD)/benchcgmt ## Time the CGMT model (N, STEPS, THREADS, CHUNK)
	$(BUILD)/benchcgmt $(N) $(STEPS) $(THREADS) $(CHUNK)

run-fgmt: $(BUILD)/benchfgmt ## Time the FGMT model (N, STEPS, FIBERS, QUANTUM)
	$(BUILD)/benchfgmt $(N) $(STEPS) $(FIBERS) $(QUANTUM)

# --- campanas completas (200 repeticiones, scripts ya calibrados) ----------
# Los scripts asumen que se invocan desde la raiz de case_2 (no desde dentro
# de scripts/), y escriben su CSV en results/raw/.

campaign-cmp: $(BUILD)/benchcmp ## Run the full CMP campaign -> results/raw/final_cmp_$(SMT).csv (SMT=on|off label)
	chmod +x $(SCRIPTS)/campaign_cmp.sh
	SMT=$(SMT) $(SCRIPTS)/campaign_cmp.sh

campaign-smt: $(BUILD)/benchsmt ## Run the full SMT campaign -> results/raw/final_smt_$(SMT).csv (SMT=on|off label)
	chmod +x $(SCRIPTS)/campaign_smt.sh
	SMT=$(SMT) WORKERS="$(SMT_WORKERS)" $(SCRIPTS)/campaign_smt.sh

rerun-smt: ## Re-measure only SMT with 8 and 16 threads (all N, 200 reps) -> results/raw/final_smt_$(SMT)_rerun.csv
	$(MAKE) campaign-smt SMT=$(SMT) SMT_WORKERS="8 16"

merge-smt-rerun: ## Compare the SMT rerun against the old SMT and CMP data (APPLY=1 replaces those rows in final_smt_$(SMT).csv)
	python3 $(SCRIPTS)/merge_smt_rerun.py $(SMT) $(if $(APPLY),--apply)

campaign-cgmt: $(BUILD)/benchcgmt ## Run the full CGMT measurement campaign -> results/raw/final_cgmt.csv
	chmod +x $(SCRIPTS)/campaign_cgmt.sh
	$(SCRIPTS)/campaign_cgmt.sh

campaign-fgmt: $(BUILD)/benchfgmt ## Run the full FGMT measurement campaign -> results/raw/final_fgmt.csv
	chmod +x $(SCRIPTS)/campaign_fgmt.sh
	$(SCRIPTS)/campaign_fgmt.sh

experiments: campaign-cmp campaign-smt campaign-cgmt campaign-fgmt ## Run all 4 campaigns with the current SMT label (what proyecto_1/Makefile calls)

# El Makefile general (proyecto_1/) llama a `sweep` en cada caso. En este
# caso el barrido por numero de hilos ya esta incluido en las campanas
# (N x hilos x repeticiones), asi que sweep es un alias de experiments.
sweep: experiments ## Thread-count sweep for scalability data (alias of experiments)

# Modo defensa: los modelos elegidos (MODELS, por defecto los 4) + baseline
# secuencial con un solo N, pocos pasos y pocas repeticiones. Verifica que todos
# den el mismo checksum y compara contra la campana completa. Mide el estado de
# SMT actual del sistema.
sweep-quick: $(BUILD)/benchcmp $(BUILD)/benchsmt $(BUILD)/benchcgmt $(BUILD)/benchfgmt ## Defense mode: short sweep (MODELS=cmp smt cgmt fgmt, N=QUICK_N, QUICK_STEPS steps, QUICK_REPS reps) -> results/quick/
	chmod +x $(SCRIPTS)/sweep_quick.sh
	N=$(QUICK_N) STEPS=$(QUICK_STEPS) REPS=$(QUICK_REPS) MODELS="$(MODELS)" SMT=$(SMT) $(SCRIPTS)/sweep_quick.sh
	python3 $(SCRIPTS)/sweep_quick_summary.py $(SMT)

# --- analisis (todo se regenera desde results/raw/*.csv) -------------------

analyze: ## Summary + plots from final_{cmp,smt}_on.csv, final_cgmt.csv, final_fgmt.csv -> summary.csv, boxplots, scalability, convergence
	python3 $(SCRIPTS)/analyze_campaign.py

analyze-smt: ## Compare SMT on vs off (CMP and SMT models) for PAPER_N -> summary_smt_on_off.csv, smt_on_off_n<N>.png
	python3 $(SCRIPTS)/compare_smt_sessions.py $(PAPER_N)

plot-affinity: ## CPU-occupancy plot (SMT on vs off) from results/perf/cpu_affinity_{on,off}.csv
	python3 $(SCRIPTS)/plot_cpu_affinity.py $(RESULTS_PERF)/cpu_affinity_on.csv $(RESULTS_PERF)/cpu_affinity_off.csv $(PAPER_N)

paper-table: ## Print paper table rows (Tp in ms, 95% CI, speedup, efficiency) for PAPER_N
	python3 $(SCRIPTS)/build_paper_table.py $(PAPER_N)

analyze-all: analyze analyze-smt plot-affinity ## Regenerate every figure and summary CSV from the saved data

# --- perfilado con perf (requisito del enunciado: comportamiento bajo SMT) -
# Todas las salidas se etiquetan con SMT=on|off (no cambian SMT en el sistema).

perf-smt: $(BUILD)/benchsmt ## perf stat on benchsmt -> results/perf/perf_smt_$(SMT).txt
	@mkdir -p $(RESULTS_PERF)
	perf stat -e cycles,instructions,context-switches,cpu-migrations \
	    $(BUILD)/benchsmt $(PAPER_N) $(STEPS) $(PERF_THREADS) 2> $(RESULTS_PERF)/perf_smt_$(SMT).txt
	@echo "Guardado en $(RESULTS_PERF)/perf_smt_$(SMT).txt"

perf-smt-thread: $(BUILD)/benchsmt ## perf stat per thread on benchsmt (attaches to the PID) -> results/perf/perf_smt_thread_$(SMT).txt
	chmod +x $(SCRIPTS)/perf_smt_thread.sh
	$(SCRIPTS)/perf_smt_thread.sh $(PAPER_N) $(STEPS) $(PERF_THREADS) $(RESULTS_PERF)/perf_smt_thread_$(SMT).txt

sample-affinity: $(BUILD)/benchsmt ## Sample which logical CPU each thread runs on -> results/perf/cpu_affinity_$(SMT).csv
	chmod +x $(SCRIPTS)/sample_cpu_affinity.sh
	$(SCRIPTS)/sample_cpu_affinity.sh $(PAPER_N) $(STEPS) $(PERF_THREADS) $(RESULTS_PERF)/cpu_affinity_$(SMT).csv

clean: ## Remove build/ (compiled binaries). Keeps results/ and sources.
	rm -rf $(BUILD)

# =================================================
# Build rules
# =================================================

$(BUILD)/nbody: Main.c Nbody.c NBody.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) Main.c Nbody.c -o $@ $(LDFLAGS)

$(BUILD)/bench: Bench.c Nbody.c NBody.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) Bench.c Nbody.c -o $@ $(LDFLAGS)

$(BUILD)/benchcmp: BenchCmp.c Nbody.c $(ENGINES)/Cmp.c NBody.h $(ENGINES)/Cmp.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) BenchCmp.c Nbody.c $(ENGINES)/Cmp.c -o $@ $(LDFLAGS)

$(BUILD)/benchsmt: BenchSmt.c Nbody.c $(ENGINES)/Cmp.c $(ENGINES)/Smt.c NBody.h $(ENGINES)/Smt.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) BenchSmt.c Nbody.c $(ENGINES)/Cmp.c $(ENGINES)/Smt.c -o $@ $(LDFLAGS)

$(BUILD)/benchcgmt: BenchCgmt.c Nbody.c $(ENGINES)/Cgmt.c NBody.h $(ENGINES)/Cgmt.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) BenchCgmt.c Nbody.c $(ENGINES)/Cgmt.c -o $@ $(LDFLAGS)

$(BUILD)/benchfgmt: BenchFgmt.c Nbody.c $(ENGINES)/Fgmt.c NBody.h $(ENGINES)/Fgmt.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) BenchFgmt.c Nbody.c $(ENGINES)/Fgmt.c -o $@ $(LDFLAGS)

$(BUILD)/test_forces: $(TESTS)/TestForces.c Nbody.c NBody.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(TESTS)/TestForces.c Nbody.c -o $@ $(LDFLAGS)

$(BUILD)/test_cmp: $(TESTS)/TestCmp.c Nbody.c $(ENGINES)/Cmp.c NBody.h $(ENGINES)/Cmp.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(TESTS)/TestCmp.c Nbody.c $(ENGINES)/Cmp.c -o $@ $(LDFLAGS)

$(BUILD)/test_cgmt: $(TESTS)/TestCgmt.c Nbody.c $(ENGINES)/Cgmt.c NBody.h $(ENGINES)/Cgmt.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(TESTS)/TestCgmt.c Nbody.c $(ENGINES)/Cgmt.c -o $@ $(LDFLAGS)

$(BUILD)/test_fgmt: $(TESTS)/TestFgmt.c Nbody.c $(ENGINES)/Fgmt.c NBody.h $(ENGINES)/Fgmt.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(TESTS)/TestFgmt.c Nbody.c $(ENGINES)/Fgmt.c -o $@ $(LDFLAGS)