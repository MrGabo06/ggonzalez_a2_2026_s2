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


# =================================================
# Targets
# =================================================

.PHONY: all help build results-dir \
        run-sequential run-fine run-coarse run-smt run-cmp \
        experiments clean

all: help

help: ## Show available targets
	@echo "Available targets:"
	@grep -E '^[a-zA-Z0-9_%./-]+:.*## ' $(MAKEFILE_LIST) | \
	awk 'BEGIN {FS = ":.*## "}; {printf "  %-20s %s\n", $$1, $$2}'
	@echo ""
	@echo "Override experiment parameters, e.g.:"
	@echo "  make run-cmp N=1024 BLOCK_SIZE=128 THREADS=8"
	@echo "  make experiments N=1024 BLOCK_SIZE=128 THREADS=8 ITER=200"

build: $(BINARIES) ## Compile the 5 programs (sequential + 4 execution models)

results-dir:
	@mkdir -p $(RESULTS)

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
	     $(BUILD)/sequential $(N) $(BLOCK_SIZE) $$((SEED+i)) | tail -n +2; \
	   done ) > $(RESULTS)/sequential.csv
	@( $(BUILD)/fineGrained $(N) $(BLOCK_SIZE) $(VTHREADS) $(SEED); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/fineGrained $(N) $(BLOCK_SIZE) $(VTHREADS) $$((SEED+i)) | tail -n +2; \
	   done ) > $(RESULTS)/fine_grained.csv
	@( $(BUILD)/coarseGrained $(N) $(BLOCK_SIZE) $(THREADS) $(SEED); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/coarseGrained $(N) $(BLOCK_SIZE) $(THREADS) $$((SEED+i)) | tail -n +2; \
	   done ) > $(RESULTS)/coarse_grained.csv
	@( $(BUILD)/smt $(N) $(BLOCK_SIZE) $(SMT_CORES) $(SMT_FACTOR) $(SEED); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/smt $(N) $(BLOCK_SIZE) $(SMT_CORES) $(SMT_FACTOR) $$((SEED+i)) | tail -n +2; \
	   done ) > $(RESULTS)/smt.csv
	@( $(BUILD)/cmp $(N) $(BLOCK_SIZE) $(THREADS) $(SEED); \
	   for i in $$(seq 2 $(ITER)); do \
	     $(BUILD)/cmp $(N) $(BLOCK_SIZE) $(THREADS) $$((SEED+i)) | tail -n +2; \
	   done ) > $(RESULTS)/cmp.csv
	@echo "Listo. Resultados en $(RESULTS)/*.csv"

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
