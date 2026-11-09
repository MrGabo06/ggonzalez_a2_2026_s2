# Makefile del framework DLP. Ejecute `make help` para ver las opciones.
 
CXX      ?= g++
NVCC     ?= nvcc
NVCCFLAGS ?= -arch=native   # compila para la GPU de esta máquina (CUDA 11.6 o más)
CUDA_PATH ?= /usr/local/cuda
 
BUILD    := build
TARGET   := $(BUILD)/dlp_bench
 
# Con `make` a secas se muestra la lista de comandos
.DEFAULT_GOAL := help
 
# Parámetros de `make run`
BACKEND  ?= seq
SIZE     ?= 1024
FILTER   ?= gauss
REPS     ?= 5
ARGS     ?=
 
CXXFLAGS := -std=c++17 -Wall -Wextra -I.
ifeq ($(DEBUG),1)
  CXXFLAGS += -O0 -g
else
  CXXFLAGS += -O2
endif
LDFLAGS  :=
SIMDFLAGS := -mavx2 -mfma
 
SRCS := src/main.cpp \
        common/image.cpp \
        common/filter.cpp \
        common/validate.cpp \
        common/stats.cpp \
        common/hex.cpp \
        backends/registry.cpp \
        backends/seq/seq_backend.cpp \
        backends/seq/seq_separable_backend.cpp \
        backends/fpga/fpga_backend.cpp
		
ifeq ($(SIMD),1)
  SRCS     += backends/simd/simd_backend.cpp
  CXXFLAGS += -DDLP_HAS_SIMD
  ifeq ($(AVX512),1)
    SRCS     += backends/simd/simd512_backend.cpp
    CXXFLAGS += -DDLP_HAS_AVX512
  endif
endif
 
CU_SRCS :=
ifeq ($(CUDA),1)
  CU_SRCS  += backends/cuda/cuda_backend.cu
  CXXFLAGS += -DDLP_HAS_CUDA
  LDFLAGS  += -L$(CUDA_PATH)/lib64 -lcudart
endif
 
OBJS := $(SRCS:%.cpp=$(BUILD)/%.o) $(CU_SRCS:%.cu=$(BUILD)/%.o)
 
.PHONY: help build clean run test test_backends bench analyze report
build: $(TARGET)
 
# Unit tests: validator and sequential baseline (no special hardware needed)
TEST_BIN  := $(BUILD)/test_validator
TEST_SRCS := tests/test_validator.cpp \
             common/image.cpp \
             common/filter.cpp \
             common/validate.cpp \
             common/stats.cpp \
             common/hex.cpp \
             backends/seq/seq_backend.cpp \
             backends/seq/seq_separable_backend.cpp
 
# Builds the test program and runs it, make fails if any test fails
test: $(TEST_BIN)
	./$(TEST_BIN)
 
$(TEST_BIN): $(TEST_SRCS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $^ -o $@
 
# Prueba todos los backends compilados contra seq (use SIMD=1, AVX512=1 o CUDA=1 para
# incluirlos). Falla si algún backend no da exactamente la misma imagen que seq.
TEST_BACKENDS_BIN := $(BUILD)/test_backends
LIB_OBJS := $(filter-out $(BUILD)/src/main.o,$(OBJS))
 
test_backends: $(TEST_BACKENDS_BIN)
	./$(TEST_BACKENDS_BIN)
 
$(TEST_BACKENDS_BIN): $(BUILD)/tests/test_backends.o $(LIB_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)
 
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
 
# Solo los archivos SIMD llevan -mavx2/-mavx512f, para que el resto del
# binario no use esas instrucciones por accidente. -mavx512f va solo en el archivo
# AVX-512, así el código AVX2 nunca usa instrucciones que una CPU sin AVX-512 no tiene.
$(BUILD)/backends/simd/simd512_backend.o: SIMDFLAGS := -mavx2 -mfma -mavx512f
 
$(BUILD)/backends/simd/%.o: backends/simd/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SIMDFLAGS) -MMD -MP -c $< -o $@
 
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
 
$(BUILD)/%.o: %.cu
	@mkdir -p $(dir $@)
	$(NVCC) -std=c++17 -O2 $(NVCCFLAGS) -I. $(filter -D%,$(CXXFLAGS)) -c $< -o $@
 
# Compila si hace falta y ejecuta el benchmark
run: $(TARGET)
	./$(TARGET) --backend $(BACKEND) --size $(SIZE) --filter $(FILTER) --reps $(REPS) $(ARGS)
 
# F4: barrido de tamaños de todos los backends compilados (genera un CSV en results/).
# Se compila con las opciones que se pasen, p. ej. make bench SIMD=1 AVX512=1 CUDA=1
# Opcional: SIZES="512 1024 2048 4096" BACKENDS="seq simd cuda" REPS=10 WARMUP=3
bench: $(TARGET)
	scripts/run_bench.sh
 
# F4: tablas y gráficas en results/figuras/ a partir de los CSV de results/
analyze:
	python3 scripts/analyze.py
 
# Barrido completo y análisis en un solo comando. Los dos pasos van en orden aunque
# se use make -j: analyze necesita el CSV que termina de escribir bench.
report:
	$(MAKE) bench
	$(MAKE) analyze
 
clean:
	rm -rf $(BUILD)
 
help:
	@echo "Uso: make <comando> [opciones]"
	@echo ""
	@echo "Comandos:"
	@echo "  make help             Muestra esta lista (es lo que sale con 'make' a secas)"
	@echo "  make build            Compila build/dlp_bench"
	@echo "  make run              Compila si hace falta y ejecuta un backend"
	@echo "  make test             Pruebas del validador y del baseline"
	@echo "  make test_backends    Compara todos los backends compilados contra seq (tolerancia 0)"
	@echo "  make bench            Barrido de tamanos de todos los backends (CSV en results/)"
	@echo "  make analyze          Tablas y graficas en results/figuras/ desde los CSV"
	@echo "  make report           Todo F4: detecta el hardware, compila, mide y genera graficas"
	@echo "  make clean            Borra la carpeta build/"
	@echo ""
	@echo "Opciones de compilacion (build, run, test_backends, bench):"
	@echo "  SIMD=1                Agrega los backends SIMD (AVX2)"
	@echo "  AVX512=1              Agrega las variantes AVX-512 (requiere SIMD=1, Intel 11va gen)"
	@echo "  CUDA=1                Agrega los backends CUDA (requiere nvcc)"
	@echo "  DEBUG=1               Compila sin optimizar y con simbolos de depuracion"
	@echo "  CUDA_PATH=<ruta>      Ruta del CUDA Toolkit (por defecto /usr/local/cuda)"
	@echo "  (make report detecta SIMD, AVX512 y CUDA solo, no hace falta pasarlas)"
	@echo ""
	@echo "Parametros de run:"
	@echo "  BACKEND=<nombre>      seq, seq_sep, simd, simd_sep, simd512, simd_sep512, cuda, cuda_tiled, fpga"
	@echo "  SIZE=<N>              Lado de la imagen sintetica (por defecto 1024)"
	@echo "  FILTER=<nombre>       gauss o sobel (por defecto gauss)"
	@echo "  REPS=<N>              Repeticiones (por defecto 5)"
	@echo "  ARGS=\"...\"            Opciones extra para dlp_bench, p. ej. ARGS=\"--validate\""
	@echo ""
	@echo "Parametros de bench y report:"
	@echo "  SIZES=\"512 1024\"      Lados de imagen a medir (por defecto 512 1024 2048 4096)"
	@echo "  BACKENDS=\"seq simd\"   Backends a medir (por defecto todos los compilados)"
	@echo "  REPS=<N> WARMUP=<N>   Repeticiones medidas y de calentamiento (por defecto 10 y 3)"
	@echo "  DLP_CUDA_BLOCK=32x8   Tamano de bloque de cuda_tiled (por defecto 16x16)"
	@echo ""
	@echo "Ejemplos:"
	@echo "  make build -j"
	@echo "  make build SIMD=1 AVX512=1 CUDA=1 -j"
	@echo "  make run BACKEND=simd SIZE=2048 SIMD=1 ARGS=\"--validate\""
	@echo "  make test_backends SIMD=1 AVX512=1 CUDA=1 -j"
	@echo "  make report"
	@echo "  make report SIZES=\"512 1024\" REPS=5"
	@echo ""
	@echo "Al cambiar SIMD, AVX512, CUDA o DEBUG hay que ejecutar make clean antes."
 
# Dependencias de headers generadas por -MMD
-include $(OBJS:.o=.d)