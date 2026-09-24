# Single Makefile — 2D heat-diffusion stencil under four execution models.
# Build: g++ / C++20 only (no CMake).

CXX      := g++
CXXSTD   := -std=c++20
WARN     := -Wall -Wextra
OPT      := -O2
# Skylake-family cores (the T480's i7-8650U) run a jump that crosses or ends on a
# 32-byte boundary outside the uop cache (JCC erratum microcode). Without padding,
# the same kernel measured 18-26% slower depending only on where it was linked.
JCC_FIX  := -Wa,-mbranches-within-32B-boundaries
CXXFLAGS := $(CXXSTD) $(OPT) $(WARN) $(JCC_FIX) -pthread
INCLUDES := -Isrc
BUILD    := build

MODELS   := baseline_seq model_cmp model_smt model_fine model_coarse
BINS     := $(addprefix $(BUILD)/,$(MODELS))
HEADERS  := src/stencil_core.hpp $(wildcard src/common/*.hpp)

# =================================================
# Targets
# NOTE: When adding rules, add what it does after
# target: requisites preceeded by ## so targets
# remain automatically documented
# =================================================

.PHONY: all help compile clean test-common dummy-fine dummy-coarse dummy-smt bench perf plots verify

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

# 2 x 8 MiB meshes overflow the 8 MiB L3, so the dummies run memory-bound and
# there is memory latency for multithreading to hide.
DUMMY_ARGS := --N 1024 --steps 500 --quiet
SAME_CHECKSUM := awk -F, '{ print } NR == 1 { ref = $$6 } $$6 != ref { bad = 1 } \
	END { if (bad) { print "FAIL: checksum differs from baseline" > "/dev/stderr"; exit 1 } }'

dummy-fine: $(BUILD)/baseline_seq $(BUILD)/model_fine ## Avance 2: fine-grained dummy, round-robin trace + quantum 1 vs 16
	./$(BUILD)/model_fine --N 16 --steps 1 --threads 4 --quantum 2 --trace
	@echo "model,N,threads,steps,time_s,checksum,switches"
	@{ ./$(BUILD)/baseline_seq $(DUMMY_ARGS); \
	   ./$(BUILD)/model_fine --N 1024 --steps 500 --threads 4 --quantum 1; \
	   ./$(BUILD)/model_fine --N 1024 --steps 500 --threads 4 --quantum 16; } | \
	$(SAME_CHECKSUM)

dummy-coarse: $(BUILD)/baseline_seq $(BUILD)/model_fine $(BUILD)/model_coarse ## Avance 2: coarse-grained dummy, yields only on predicted misses vs fine
	./$(BUILD)/model_coarse --N 64 --steps 2 --threads 4 --trace
	./$(BUILD)/model_coarse --N 1024 --steps 1 --threads 4 --trace --quiet > /dev/null
	@echo "model,N,threads,steps,time_s,checksum,switches"
	@{ ./$(BUILD)/baseline_seq $(DUMMY_ARGS); \
	   ./$(BUILD)/model_fine --N 1024 --steps 500 --threads 4 --quantum 1; \
	   ./$(BUILD)/model_coarse --N 1024 --steps 500 --threads 4; } | \
	$(SAME_CHECKSUM)

dummy-smt: $(BUILD)/baseline_seq $(BUILD)/model_cmp $(BUILD)/model_smt ## Avance 2/3.3: SMT vs CMP at equal cores (packed siblings vs one thread per core)
	@echo "model,N,threads,steps,time_s,checksum,switches"
	@{ ./$(BUILD)/baseline_seq $(DUMMY_ARGS); \
	   ./$(BUILD)/model_cmp $(DUMMY_ARGS) --threads 1; \
	   ./$(BUILD)/model_smt $(DUMMY_ARGS) --threads 2; \
	   ./$(BUILD)/model_cmp $(DUMMY_ARGS) --threads 4; \
	   ./$(BUILD)/model_smt $(DUMMY_ARGS) --threads 8; } | \
	$(SAME_CHECKSUM)

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

verify: compile $(BUILD)/verify_mesh ## Check every model's output mesh against the baseline (exact match)
	BUILD=$(BUILD) ./tests/verify.sh

$(BUILD)/verify_mesh: tests/verify_mesh.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<
