# =================================================
# Configuration: Ray Tracer (esferas + Phong + reflexiones) —
# Multithreading Execution Model Framework (CE4302) — case_4
#
# =================================================

# C++ 
CXX      := g++
# -O2: optimizacion moderada (mediciones realistas).
CXXFLAGS := -O2 -Wall -Wextra -std=c++17 -Isrc
# -pthread: necesario para enlazar la libreria de hilos POSIX (pthread_create, etc.).
LDFLAGS  := -pthread

SRC_DIR     := src
ENGINES_DIR := engines
BUILD       := build
RESULTS     := results

COMMON_SRC := $(wildcard $(SRC_DIR)/*.cpp)
COMMON_OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD)/%.o,$(COMMON_SRC))
COMMON_HDR := $(wildcard $(SRC_DIR)/*.h)

# Cada engines/X.cpp produce un ejecutable build/X (sequential,
# fineGrained, coarseGrained, smt, cmp) segun los archivos que existan.
ENGINE_SRC := $(wildcard $(ENGINES_DIR)/*.cpp)
BINARIES   := $(patsubst $(ENGINES_DIR)/%.cpp,$(BUILD)/%,$(ENGINE_SRC))


# =================================================
# Targets
# =================================================

.PHONY: all help build clean distclean clean-images bench bench-oficial analyze run perf

# Sin esto, make trata build/*.o como archivos "intermedios" y los borra
# apenas termina de enlazar. .SECONDARY los conserva para no recompilar
# el codigo comun en cada `make build`.
.SECONDARY: $(COMMON_OBJ)

all: help

help: ## Show available targets
	@echo "Available targets:"
	@grep -E '^[a-zA-Z0-9_%./-]+:.*## ' $(MAKEFILE_LIST) | \
	awk 'BEGIN {FS = ":.*## "}; {printf "  %-20s %s\n", $$1, $$2}'

build: $(BINARIES) ## Compile every engine found in engines/

clean: ## Remove build artifacts only (results/ se conserva)
	@rm -rf $(BUILD)

# results/ guarda las 200 corridas oficiales y las capturas de perf, que NO
# estan en git (results/ esta en .gitignore): borrarlo es irreversible.
distclean: clean ## Remove build artifacts AND results/ (irreversible)
	@rm -rf $(RESULTS)

clean-images: ## Borra solo las imagenes de `make run` (results/run_*.bmp); no toca CSVs ni perf
	@rm -f $(RESULTS)/run_*.bmp

# bench/perf no dependen de `build` (que siempre incluye smt) porque smt solo
# compila en Linux (afinidad de CPU). Se piden los 4 binarios portables y,
# solo en Linux, tambien smt (mas abajo); en macOS bench.py omite smt.
BENCH_BINARIES := $(BUILD)/sequential $(BUILD)/coarseGrained $(BUILD)/fineGrained $(BUILD)/cmp

# smt usa afinidad de CPU (pthread_setaffinity_np): solo compila en Linux.
ifeq ($(shell uname -s),Linux)
BENCH_BINARIES += $(BUILD)/smt
endif

bench: $(BENCH_BINARIES) ## Corre scripts/bench.py (prueba corta, 5 corridas). Incluye smt en Linux con SMT.
	python3 scripts/bench.py

bench-oficial: $(BENCH_BINARIES) ## Corre scripts/bench.py con 200 corridas -> results/bench_oficial.csv
	python3 scripts/bench.py --runs 200 --out results/bench_oficial.csv

# CSV por defecto para `make analyze`; cambialo con: make analyze CSV=otro.csv
CSV ?= results/bench_oficial.csv

perf: $(BENCH_BINARIES) ## Captura contadores de hardware con perf (Linux fisico) -> results/perf/*.txt
	@bash scripts/perf_capture.sh

analyze: ## Corre scripts/analyze.py sobre un CSV (uso: make analyze CSV=results/bench_oficial.csv)
	python3 scripts/analyze.py $(CSV)

run: $(BENCH_BINARIES) ## Corre cada motor una vez (control: compara imagenes contra sequential) - ver scripts/run_all.sh
	@$(MAKE) $(BUILD)/smt 2>/dev/null || true
	@bash scripts/run_all.sh


# =================================================
# Build rules
# =================================================

# Regla generica: src/foo.cpp -> build/foo.o (objeto comun).
$(BUILD)/%.o: $(SRC_DIR)/%.cpp $(COMMON_HDR)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Regla generica: engines/foo.cpp + objetos comunes -> build/foo.
$(BUILD)/%: $(ENGINES_DIR)/%.cpp $(COMMON_OBJ) $(COMMON_HDR)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $< $(COMMON_OBJ) -o $@ $(LDFLAGS)