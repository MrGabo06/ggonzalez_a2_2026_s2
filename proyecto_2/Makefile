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
  SRCS     += backends/simd/simd_backend.cpp \
              backends/simd/simd_separable_backend.cpp
  CXXFLAGS += -DDLP_HAS_SIMD
  ifeq ($(AVX512),1)
    SRCS     += backends/simd/simd512_backend.cpp \
                backends/simd/simd512_separable_backend.cpp
    CXXFLAGS += -DDLP_HAS_AVX512
  endif
endif

CU_SRCS :=
ifeq ($(CUDA),1)
  CU_SRCS  += backends/cuda/cuda_backend.cu \
              backends/cuda/cuda_tiled_backend.cu
  CXXFLAGS += -DDLP_HAS_CUDA
  LDFLAGS  += -L$(CUDA_PATH)/lib64 -lcudart
endif

OBJS := $(SRCS:%.cpp=$(BUILD)/%.o) $(CU_SRCS:%.cu=$(BUILD)/%.o)

.PHONY: help build clean clean_results run test test_backends bench analyze report profile profile_cpu profile_gpu fpga_sim fpga_test fpga_wave fpga_board fpga_board_sim fpga_jtag_sim fpga_jtag_ping fpga_jtag_test fpga_jtag_bench fpga_signaltap_report fpga_baseline compare numeric_export numeric_diff efficiency fpga_resources bandwidth
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
$(BUILD)/backends/simd/simd512%.o: SIMDFLAGS := -mavx2 -mfma -mavx512f

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

# ---- F4: mediciones ----------------------------------------------------------------
# make report   limpia, detecta qué soporta esta máquina (AVX2, AVX-512, GPU NVIDIA),
#               compila con eso, mide todos los backends y genera tablas y gráficas.
# make bench    solo mide (usa las opciones que se pasen: SIMD=1 AVX512=1 CUDA=1)
# make analyze  solo tablas y gráficas, desde los CSV que haya en results/
# Opcional: SIZES="512 1024 2048 4096" BACKENDS="seq simd cuda" REPS=10 WARMUP=3
HAS_AVX2   := $(shell grep -qw avx2 /proc/cpuinfo 2>/dev/null && echo 1)
HAS_AVX512 := $(shell grep -qw avx512f /proc/cpuinfo 2>/dev/null && echo 1)
HAS_CUDA   := $(shell command -v $(NVCC) >/dev/null 2>&1 && nvidia-smi -L >/dev/null 2>&1 && echo 1)
AUTO_FLAGS := $(if $(HAS_AVX2),SIMD=1) $(if $(and $(HAS_AVX2),$(HAS_AVX512)),AVX512=1) $(if $(HAS_CUDA),CUDA=1)
JOBS       ?= $(shell nproc 2>/dev/null || echo 1)
BENCH_HOST := $(shell hostname)

bench: $(TARGET)
	scripts/run_bench.sh

analyze:
	python3 scripts/analyze.py

# Los pasos van en orden aunque se use make -j (analyze necesita el CSV de bench).
# Se hace clean antes para que el binario salga con las opciones detectadas, y se borran
# los resultados anteriores de ESTA máquina para que quede una sola corrida. Los CSV de
# las otras máquinas (otro hostname) no se tocan.
report:
	@echo "Opciones detectadas: $(AUTO_FLAGS)"
	$(MAKE) clean
	rm -f results/$(BENCH_HOST)_*.csv results/figuras/$(BENCH_HOST)_*
	$(MAKE) -j$(JOBS) bench $(AUTO_FLAGS)
	$(MAKE) analyze

# ---- F4: perfilado -------------------------------------------------------------------
# make profile      perf (CPU) y Nsight Compute (GPU): detecta el hardware, compila y perfila.
#                   Escribe results/perfilado/<maquina>_perfilado.md. Puede pedir la contrasena
#                   de sudo (contadores de CPU o de GPU). Lo que la maquina no tenga se omite.
# make profile_cpu  solo perf          make profile_gpu  solo Nsight Compute
# Opcional: PROF_SIZES="1024 4096" PROF_GPU_SIZE=4096 PROF_BACKENDS="seq simd"
PROFILE_MODE ?= all

profile:
	@echo "Opciones detectadas: $(AUTO_FLAGS)"
	$(MAKE) clean
	@case "$(PROFILE_MODE)" in \
	  cpu) rm -f results/perfilado/$(BENCH_HOST)_perf.csv ;; \
	  gpu) rm -rf results/perfilado/$(BENCH_HOST)_ncu ;; \
	  *)   rm -rf results/perfilado/$(BENCH_HOST)_* ;; \
	esac
	$(MAKE) -j$(JOBS) build $(AUTO_FLAGS)
	bash scripts/run_profile.sh $(PROFILE_MODE)

profile_cpu:
	$(MAKE) profile PROFILE_MODE=cpu

profile_gpu:
	$(MAKE) profile PROFILE_MODE=gpu

# ---- F5: procesador vectorial para la FPGA (simulación con Icarus Verilog) ---------------
# make fpga_sim   ensambla el programa, simula el RTL y compara contra la salida de seq
# make fpga_test  prueba del ISA + ejecución dummy + Gauss y Sobel en dos tamaños
FPGA_SIM := python3 fpga/scripts/run_sim.py

fpga_sim: $(TARGET)
	$(FPGA_SIM) --filter $(FILTER) --size $(if $(filter 1024,$(SIZE)),32,$(SIZE))

# Simula con formas de onda y abre gtkwave con la vista ya armada (por defecto el caso smoke)
fpga_wave: $(TARGET)
	$(FPGA_SIM) --filter $(if $(filter command line,$(origin FILTER)),$(FILTER),smoke) --size $(if $(filter 1024,$(SIZE)),32,$(SIZE)) --wave

# DE1-SoC: exporta el caso elegido a fpga/board/data/ (lo que la tarjeta carga al encender)
# y simula el top completo de la tarjeta (carga, ejecución con KEY[1], comparación, displays)
fpga_board: $(TARGET)
	$(FPGA_SIM) --filter $(FILTER) --size $(if $(filter 1024,$(SIZE)),32,$(SIZE)) --board

fpga_board_sim:
	@mkdir -p $(BUILD)/fpga
	cd fpga/board && iverilog -g2005 -Wall -Wno-timescale -I ../rtl -I data \
		-o ../../$(BUILD)/fpga/tb_de1soc_top.vvp tb_de1soc_top.v de1soc_top.v de1soc_display.v ../rtl/vproc_*.v
	cd fpga/board && vvp -n ../../$(BUILD)/fpga/tb_de1soc_top.vvp

# DE1-SoC por JTAG (fpga/jtag/): la PC manda la imagen, el VPROC la procesa y la PC lee el
# resultado. fpga_jtag_sim prueba todo el camino sin tarjeta (C++ -> Tcl -> RTL en Icarus);
# DMEM pequeño para que la imagen se procese en varias franjas, como pasa con las grandes.
FPGA_COSIM := python3 fpga/jtag/sim/cosim.py --
fpga_jtag_sim: $(TARGET)
	DLP_FPGA_DMEM=2048 $(FPGA_COSIM) ./$(TARGET) --backend fpga --filter gauss --size 37 --reps 1 --warmup 1 --validate --output $(BUILD)/fpga/jtag_gauss.pgm
	DLP_FPGA_DMEM=2048 $(FPGA_COSIM) ./$(TARGET) --backend fpga --filter sobel --size 40 --reps 1 --warmup 0 --validate --output $(BUILD)/fpga/jtag_sobel.pgm
	$(FPGA_COSIM) ./$(TARGET) --backend fpga --filter gauss --size 32 --reps 1 --warmup 0 --validate --output $(BUILD)/fpga/jtag_gauss32.pgm

# Con la tarjeta programada con fpga/jtag/output_files/vproc_jtag.sof
QUARTUS_STP ?= $(shell command -v quartus_stp 2>/dev/null || ls -d $$HOME/altera_lite/*/quartus/bin/quartus_stp $$HOME/intelFPGA_lite/*/quartus/bin/quartus_stp 2>/dev/null | head -1)
fpga_jtag_ping:
	@test -n "$(QUARTUS_STP)" || (echo "No se encontró quartus_stp; use make fpga_jtag_ping QUARTUS_STP=/ruta/quartus_stp" && false)
	$(QUARTUS_STP) -t fpga/jtag/vproc_jtag.tcl ping

fpga_jtag_test: $(TARGET)
	./$(TARGET) --backend fpga --filter gauss --size 32 --reps 3 --warmup 1 --validate
	./$(TARGET) --backend fpga --filter sobel --size 32 --reps 3 --warmup 1 --validate
	./$(TARGET) --backend fpga --filter gauss --size 100 --reps 3 --warmup 1 --validate
	./$(TARGET) --backend fpga --filter sobel --size 256 --reps 3 --warmup 1 --validate

# Con la tarjeta programada con vproc_jtag.sof: barrido de filtros y tamanos con --validate,
# guarda un CSV nuevo en results/fpga/ e imprime la tabla de resultados.
# Opcional: FPGA_SIZES="32 64 100 128 256 512" FPGA_FILTERS="gauss sobel" FPGA_REPS=5 FPGA_WARMUP=1
fpga_jtag_bench: $(TARGET)
	scripts/run_fpga_bench.sh

# Perfil interno del VPROC (ocupacion de la FSM, CPI, mezcla de instrucciones, bucle interno) a
# partir de un CSV exportado de SignalTap con la revision vproc_jtag_stp (fpga/jtag/vproc_jtag.stp).
# Escribe el reporte junto al CSV con sufijo _perfil.md. STP_CYCLES (opcional) cruza con el
# contador de hardware. Ejemplo: make fpga_signaltap_report STP_CSV=results/signaltap/sobel_32.csv STP_CYCLES=9942
STP_CSV    ?= results/signaltap/sobel_32.csv
STP_CYCLES ?=
fpga_signaltap_report:
	python3 scripts/signaltap_report.py $(STP_CSV) $(if $(STP_CYCLES),--cycles $(STP_CYCLES)) --md $(STP_CSV:.csv=_perfil.md)

# ---- Comparativa CPU, GPU y FPGA ---------------------------------------------------------
# make fpga_baseline  mide seq y SIMD en ESTA maquina a los tamanos de la FPGA (32 a 1024); es la
#                     base para el speedup de la FPGA. Hacerlo en la maquina que maneja la tarjeta.
# make compare        tablas y graficas con la FPGA junto a CPU y GPU (results/figuras/comparativa_*)
BASELINE_BACKENDS := seq seq_sep simd simd_sep simd512 simd_sep512
BASELINE_OUT      := results/fpga/$(BENCH_HOST)_baseline.csv

fpga_baseline:
	$(MAKE) clean
	$(MAKE) -j$(JOBS) build $(AUTO_FLAGS)
	@mkdir -p results/fpga
	rm -f $(BASELINE_OUT)
	BACKENDS="$(BASELINE_BACKENDS)" SIZES="32 64 100 128" REPS=200 WARMUP=10 OUT=$(BASELINE_OUT) scripts/run_bench.sh
	BACKENDS="$(BASELINE_BACKENDS)" SIZES="256 512 1024" REPS=40 WARMUP=5 OUT=$(BASELINE_OUT) scripts/run_bench.sh

compare:
	python3 scripts/compare_platforms.py

# make efficiency   eficiencia paralela de CUDA y de la FPGA (clasica S/p y utilizacion del pico);
#                   usa los CSV de results/ y los reportes de Nsight Compute de results/perfilado/
efficiency:
	python3 scripts/parallel_efficiency.py

# make bandwidth    ancho de banda de memoria de CPU, GPU y FPGA derivado de lo medido
#                   (results/figuras/ancho_de_banda.*); la DRAM de la GPU sale de Nsight Compute
bandwidth:
	python3 scripts/memory_bandwidth.py

# make fpga_resources  recursos y Fmax de Quartus (results/quartus/): lee los reportes de
#                      fpga/board/output_files y fpga/jtag/output_files, que hay que compilar antes en Quartus
fpga_resources:
	python3 fpga/scripts/quartus_report.py

# make numeric_export  en CADA maquina (detecta SIMD, AVX-512, GPU y la FPGA): guarda los digests de
#                      las salidas en results/numeric/<maquina>_digest.csv (eso es lo que se sube a git)
# make numeric_diff    une los digests de todas las maquinas en results/numeric/diferencias.md
numeric_export:
	$(MAKE) clean
	$(MAKE) -j$(JOBS) build $(AUTO_FLAGS)
	python3 scripts/numeric_diff.py export

numeric_diff:
	python3 scripts/numeric_diff.py report

fpga_test: $(TARGET)
	$(FPGA_SIM) --filter isa
	$(FPGA_SIM) --filter smoke
	$(FPGA_SIM) --filter gauss --size 32
	$(FPGA_SIM) --filter sobel --size 32
	$(FPGA_SIM) --filter gauss --size 64 --seed 7
	$(FPGA_SIM) --filter sobel --size 64 --seed 7

# ---- Demo: filter a photo with every backend ---------------------------------------------
# Drop a photo in examples/ (git ignores its contents) and run:
#   make demo            seq, SIMD and CUDA backends that are compiled in build/dlp_bench
#   make demo FPGA=1     the same plus the DE1-SoC (board programmed with vproc_jtag.sof)
# Or run one family at a time: demo_seq, demo_simd, demo_cuda, demo_fpga, and finish with
# demo_images. Outputs go to examples/output/<photo name>/ (not build/, so `make clean` keeps them).
# Optional: DEMO_IMAGE=<path> DEMO_SIZE=512 DEMO_REPS=1 DEMO_WARMUP=0 DEMO_FILTERS="gauss sobel"
DEMO_IMAGE   ?=
DEMO_SIZE    ?= 512
DEMO_REPS    ?= 1
DEMO_WARMUP  ?= 0
DEMO_FILTERS ?= gauss sobel
DEMO_OPTIONAL ?= 0
FPGA         ?= 0
DEMO_RUN := BIN=$(TARGET) DEMO_IMAGE="$(DEMO_IMAGE)" DEMO_SIZE=$(DEMO_SIZE) DEMO_REPS=$(DEMO_REPS) \
            DEMO_WARMUP=$(DEMO_WARMUP) DEMO_FILTERS="$(DEMO_FILTERS)" DEMO_OPTIONAL=$(DEMO_OPTIONAL) \
            scripts/run_demo.sh

.PHONY: demo demo_prepare demo_seq demo_simd demo_cuda demo_fpga demo_images

demo_prepare:
	@$(DEMO_RUN) prepare

demo_seq: $(TARGET)
	@$(DEMO_RUN) seq

demo_simd: $(TARGET)
	@$(DEMO_RUN) simd

demo_cuda: $(TARGET)
	@$(DEMO_RUN) cuda

# fpga_jtag_ping first, so a missing board or the wrong .sof fails right away
demo_fpga: $(TARGET) fpga_jtag_ping
	@$(DEMO_RUN) fpga

demo_images:
	@$(DEMO_RUN) images

# The steps run one after another (not as prerequisites) so make -j cannot overlap the board
# access, pollute the CPU timings or render the montage before the outputs exist. A family that
# is not compiled is skipped, and demo_images runs last even if a backend failed.
demo: $(TARGET)
	@status=0; \
	$(MAKE) --no-print-directory demo_prepare || exit 1; \
	$(MAKE) --no-print-directory demo_seq DEMO_OPTIONAL=1 || status=1; \
	$(MAKE) --no-print-directory demo_simd DEMO_OPTIONAL=1 || status=1; \
	$(MAKE) --no-print-directory demo_cuda DEMO_OPTIONAL=1 || status=1; \
	if [ "$(FPGA)" = "1" ]; then \
	  $(MAKE) --no-print-directory demo_fpga || status=1; \
	else \
	  echo "[fpga] omitido (use make demo FPGA=1 con la tarjeta programada)"; \
	fi; \
	$(MAKE) --no-print-directory demo_images || status=1; \
	exit $$status

# Borra TODOS los resultados, también los de otras máquinas (cuidado si ya los commiteó)
clean_results:
	rm -rf results

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
	@echo "                        (borra antes los resultados anteriores de esta maquina)"
	@echo "  make profile          Perfilado: perf (CPU) y Nsight Compute (GPU), resumen en results/perfilado/"
	@echo "  make profile_cpu      Solo perf          make profile_gpu   Solo Nsight Compute"
	@echo "  make fpga_sim         Simula el procesador vectorial de la FPGA (FILTER, SIZE; por defecto 32)"
	@echo "                        FILTER tambien acepta smoke (ejecucion dummy) e isa (prueba del ISA)"
	@echo "  make fpga_wave        Simula y abre gtkwave con las senales del procesador (por defecto smoke)"
	@echo "  make fpga_board       Prepara fpga/board/data/ para la DE1-SoC (FILTER, SIZE; por defecto gauss 32)"
	@echo "  make fpga_board_sim   Simula el top de la DE1-SoC con esos datos (debe terminar en PASS)"
	@echo "  make fpga_jtag_sim    Prueba el backend fpga por JTAG sin tarjeta (requiere iverilog y tclsh)"
	@echo "  make fpga_jtag_ping   Con la tarjeta programada (fpga/jtag): verifica que el puente JTAG responde"
	@echo "  make fpga_jtag_test   Con la tarjeta: corre el backend fpga y valida contra seq"
	@echo "  make fpga_jtag_bench  Con la tarjeta: barrido de filtros y tamanos, CSV en results/fpga/ y tabla"
	@echo "                        (FPGA_SIZES=\"32 64 100\" FPGA_FILTERS=gauss FPGA_REPS=5 FPGA_WARMUP=1)"
	@echo "  make fpga_signaltap_report  Perfil del VPROC desde un CSV de SignalTap (STP_CSV=<archivo> STP_CYCLES=<ciclos>)"
	@echo "  make fpga_baseline    Mide seq y SIMD en esta maquina a los tamanos de la FPGA (32 a 1024)"
	@echo "  make compare          Tablas y graficas con la FPGA junto a CPU y GPU (results/figuras/comparativa_*)"
	@echo "  make efficiency       Eficiencia paralela de CUDA y de la FPGA (results/figuras/eficiencia_paralela.*)"
	@echo "  make bandwidth        Ancho de banda de memoria de CPU, GPU y FPGA (results/figuras/ancho_de_banda.*)"
	@echo "  make fpga_resources   Recursos y Fmax de Quartus de los disenos de la FPGA (results/quartus/)"
	@echo "  make numeric_export   Digests de las salidas de los backends de esta maquina (results/numeric/)"
	@echo "  make numeric_diff     Une los digests de todas las maquinas: diferencias numericas CPU, GPU y FPGA"
	@echo "  make fpga_test        Prueba del ISA, dummy, Gauss y Sobel en simulacion (requiere iverilog)"
	@echo "  make demo             Filtra una foto de examples/ con todos los backends compilados y arma el montaje"
	@echo "                        (agregue FPGA=1 para incluir la tarjeta). Salidas en examples/output/<foto>/"
	@echo "  make demo_seq         Solo seq y seq_sep       make demo_simd   Solo los SIMD compilados"
	@echo "  make demo_cuda        Solo cuda y cuda_tiled   make demo_fpga   Solo la FPGA (hace fpga_jtag_ping antes)"
	@echo "  make demo_images      PNG de cada salida, montaje.png y comparacion byte a byte (correr al final)"
	@echo "  make clean            Borra la carpeta build/"
	@echo "  make clean_results    Borra la carpeta results/ completa (todas las maquinas)"
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
	@echo "Parametros de demo:"
	@echo "  DEMO_IMAGE=<ruta>     Foto a filtrar (por defecto, la unica imagen de examples/)"
	@echo "  DEMO_SIZE=<N>         Lado del recorte cuadrado en gris que reciben los backends (por defecto 512)"
	@echo "  DEMO_REPS=<N> DEMO_WARMUP=<N>   Repeticiones y calentamiento (por defecto 1 y 0)"
	@echo "  FPGA=1                Incluye la FPGA en make demo"
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