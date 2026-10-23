# Makefile del framework DLP. Ejecute `make help` para ver las opciones.

CXX      ?= g++
NVCC     ?= nvcc
CUDA_PATH ?= /usr/local/cuda

BUILD    := build
TARGET   := $(BUILD)/dlp_bench

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
        backends/registry.cpp \
        backends/seq/seq_backend.cpp \
        backends/fpga/fpga_backend.cpp

ifeq ($(SIMD),1)
  SRCS     += backends/simd/simd_backend.cpp
  CXXFLAGS += -DDLP_HAS_SIMD
  ifeq ($(AVX512),1)
    CXXFLAGS  += -DDLP_HAS_AVX512
    SIMDFLAGS += -mavx512f
  endif
endif

CU_SRCS :=
ifeq ($(CUDA),1)
  CU_SRCS  += backends/cuda/cuda_backend.cu
  CXXFLAGS += -DDLP_HAS_CUDA
  LDFLAGS  += -L$(CUDA_PATH)/lib64 -lcudart
endif

OBJS := $(SRCS:%.cpp=$(BUILD)/%.o) $(CU_SRCS:%.cu=$(BUILD)/%.o)

.PHONY: build clean help run test
build: $(TARGET)

# Unit tests: validator and sequential baseline (no special hardware needed)
TEST_BIN  := $(BUILD)/test_validator
TEST_SRCS := tests/test_validator.cpp \
             common/image.cpp \
             common/filter.cpp \
             common/validate.cpp \
             backends/seq/seq_backend.cpp

# Builds the test program and runs it, make fails if any test fails
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Solo el archivo SIMD lleva -mavx2/-mavx512f, para que el resto del
# binario no use esas instrucciones por accidente.
$(BUILD)/backends/simd/%.o: backends/simd/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SIMDFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.cu
	@mkdir -p $(dir $@)
	$(NVCC) -std=c++17 -O2 -I. $(filter -D%,$(CXXFLAGS)) -c $< -o $@

# Compila si hace falta y ejecuta el benchmark
run: $(TARGET)
	./$(TARGET) --backend $(BACKEND) --size $(SIZE) --filter $(FILTER) --reps $(REPS) $(ARGS)

clean:
	rm -rf $(BUILD)

help:
	@echo "Uso: make [objetivo] [opciones]"
	@echo ""
	@echo "Objetivos:"
	@echo "  build (por defecto) Compila build/dlp_bench"
	@echo "  run                Compila si hace falta y ejecuta dlp_bench"
	@echo "  test               Compila y ejecuta las pruebas del validador y del baseline"
	@echo "  clean              Borra la carpeta build/"
	@echo "  help               Muestra esta ayuda"
	@echo ""
	@echo "Opciones:"
	@echo "  SIMD=1             Agrega el backend SIMD (AVX2)"
	@echo "  AVX512=1           Agrega variantes AVX-512 (requiere SIMD=1, Intel 11va gen)"
	@echo "  CUDA=1             Agrega el backend CUDA (requiere nvcc)"
	@echo "  DEBUG=1            Compila sin optimizar y con simbolos de depuracion"
	@echo "  CUDA_PATH=<ruta>   Ruta del CUDA Toolkit (por defecto /usr/local/cuda)"
	@echo ""
	@echo "Parametros de run:"
	@echo "  BACKEND=<nombre>   seq, simd, cuda o fpga (por defecto seq)"
	@echo "  SIZE=<N>           Lado de la imagen sintetica (por defecto 1024)"
	@echo "  FILTER=<nombre>    gauss o sobel (por defecto gauss)"
	@echo "  REPS=<N>           Repeticiones (por defecto 5)"
	@echo "  ARGS=\"...\"         Opciones extra para dlp_bench, p. ej. ARGS=\"--input img.pgm\""
	@echo ""
	@echo "Ejemplos:"
	@echo "  make -j"
	@echo "  make SIMD=1 AVX512=1 -j"
	@echo "  make clean && make CUDA=1 -j"
	@echo "  make run"
	@echo "  make run BACKEND=simd SIZE=2048 SIMD=1"
	@echo "  make run FILTER=sobel ARGS=\"--input out.pgm --output bordes.pgm\""
	@echo ""
	@echo "Al cambiar SIMD, AVX512, CUDA o DEBUG hay que ejecutar make clean antes."

# Dependencias de headers generadas por -MMD
-include $(OBJS:.o=.d)