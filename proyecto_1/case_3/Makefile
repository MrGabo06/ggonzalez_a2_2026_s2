# Single Makefile — 2D heat-diffusion stencil under four execution models.
# Build: g++ / C++20 only (no CMake).

CXX      := g++
CXXSTD   := -std=c++20
WARN     := -Wall -Wextra
OPT      := -O2
CXXFLAGS := $(CXXSTD) $(OPT) $(WARN) -pthread
INCLUDES := -Isrc
BUILD    := build

# Pending models (Phases 2-3): model_smt model_coarse model_fine
MODELS   := baseline_seq model_cmp
BINS     := $(addprefix $(BUILD)/,$(MODELS))
HEADERS  := src/stencil_core.hpp $(wildcard src/common/*.hpp)

# =================================================
# Targets
# NOTE: When adding rules, add what it does after
# target: requisites preceeded by ## so targets
# remain automatically documented
# =================================================

.PHONY: all help compile clean test-common dummy-smt bench perf plots verify

all: help

help: ## Show this help
	@echo "Available targets:"
	@grep -E '^[a-zA-Z0-9_%-.]+:.*?## ' $(MAKEFILE_LIST) | \
	awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

compile: $(BINS) ## Compile all model binaries

$(BUILD)/%: src/%.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

test-common: $(BUILD)/test_common ## Run common-utility and kernel tests (ASan/UBSan)
	./$(BUILD)/test_common

$(BUILD)/test_common: tests/test_common.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -fsanitize=address,undefined -o $@ $<

# 2 x 8 MiB meshes overflow the 8 MiB L3, so the run is memory-bound and SMT
# siblings have latency to hide.
DUMMY_ARGS := --N 1024 --steps 500 --quiet

dummy-smt: $(BUILD)/baseline_seq $(BUILD)/model_cmp ## Avance 2: SMT dummy, CMP with T=4 (one per core) vs T=8 (on siblings)
	@echo "model,N,threads,steps,time_s,checksum"
	@{ ./$(BUILD)/baseline_seq $(DUMMY_ARGS); \
	   ./$(BUILD)/model_cmp $(DUMMY_ARGS) --threads 4; \
	   ./$(BUILD)/model_cmp $(DUMMY_ARGS) --threads 8; } | \
	awk -F, '{ print } NR == 1 { ref = $$6 } $$6 != ref { bad = 1 } \
	         END { if (bad) { print "FAIL: checksum differs from baseline" > "/dev/stderr"; exit 1 } }'

$(BUILD):
	mkdir -p $(BUILD)

clean: ## Remove build artifacts
	rm -rf $(BUILD)

bench: ## Phase 4: >=200 runs per configuration (needs physical hardware)
	@echo "[TODO Phase 4] bench/run_all.sh (needs physical hardware)"

perf: ## Phase 4: perf capture, per logical thread (needs physical hardware)
	@echo "[TODO Phase 4] bench/perf_capture.sh (needs physical hardware)"

plots: ## Phase 5: generate plots from collected CSVs
	@echo "[TODO Phase 5] python3 analysis/plots.py"

verify: ## Phase 3: verify the four models against the baseline
	@echo "[TODO Phase 3] make verify"
