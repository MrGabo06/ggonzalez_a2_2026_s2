# =================================================
# Configuration: Ray Tracer (esferas + Phong + reflexiones) —
# Multithreading Execution Model Framework (CE4302) — case_4
#
# =================================================

# C++ 
CXX      := g++
# -O2: optimizacion moderada (mediciones realistas).
CXXFLAGS := -O2 -Wall -Wextra -std=c++17 -Isrc
# -pthread: necesario para enlazar std::thread en Linux.
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

.PHONY: all help build clean bench bench-oficial analyze

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

clean: ## Remove build artifacts and results
	@rm -rf $(BUILD) $(RESULTS)

# bench no depende de `build` (que incluye smt) porque smt solo compila en
# Linux con afinidad real; bench.py no lo usa, asi que solo pedimos los 4
# binarios que si corren en cualquier plataforma (incluye macOS).
BENCH_BINARIES := $(BUILD)/sequential $(BUILD)/coarseGrained $(BUILD)/fineGrained $(BUILD)/cmp

bench: $(BENCH_BINARIES) ## Corre scripts/bench.py (prueba corta, 5 corridas). No incluye smt.
	python3 scripts/bench.py

bench-oficial: $(BENCH_BINARIES) ## Corre scripts/bench.py con 200 corridas -> results/bench_oficial.csv
	python3 scripts/bench.py --runs 200 --out results/bench_oficial.csv

# CSV por defecto para `make analyze`; cambialo con: make analyze CSV=otro.csv
CSV ?= results/bench_oficial.csv

analyze: ## Corre scripts/analyze.py sobre un CSV (uso: make analyze CSV=results/bench_oficial.csv)
	python3 scripts/analyze.py $(CSV)


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
