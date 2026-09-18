# Single Makefile — 2D heat-diffusion stencil under four execution models.
# Build: g++ / C++20 only (no CMake).

CXX      := g++
CXXSTD   := -std=c++20
WARN     := -Wall -Wextra
OPT      := -O2
CXXFLAGS := $(CXXSTD) $(OPT) $(WARN) -pthread
INCLUDES := -Isrc
BUILD    := build

# Pending models (Phases 2-3): model_cmp model_smt model_coarse model_fine
MODELS   := baseline_seq
BINS     := $(addprefix $(BUILD)/,$(MODELS))
HEADERS  := src/stencil_core.hpp $(wildcard src/common/*.hpp)

# =================================================
# Targets
# NOTE: When adding rules, add what it does after
# target: requisites preceeded by ## so targets
# remain automatically documented
# =================================================

.PHONY: all help compile clean test-common bench perf plots verify

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
