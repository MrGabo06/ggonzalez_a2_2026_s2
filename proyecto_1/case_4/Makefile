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

.PHONY: all help build clean

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
