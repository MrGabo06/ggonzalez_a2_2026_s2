# =================================================
# Makefile general — proyecto_1
#
# Este Makefile NO compila nada por si mismo. Su unico trabajo es
# delegar (make -C) cada target hacia el Makefile propio de cada
# case_X/, que es quien realmente sabe con que compilador, estandar de
# C/C++ y flags debe construirse ese caso. Cada invocacion de sub-make corre en su propio
# directorio de trabajo, con sus propias variables y su propia carpeta
# build/, sin que un caso vea ni pise las reglas de otro.
#
# =================================================

# Detecta automaticamente todas las carpetas case_* que EXISTAN y que
# ademas contengan su propio Makefile (asi no falla si alguien crea una
# carpeta case_X vacia antes de tener codigo adentro).
ALL_CASES := $(sort $(notdir $(patsubst %/,%,$(dir $(wildcard case_*/Makefile)))))

# CASE=case_1 restringe la operacion a un solo caso; por defecto corre
# sobre todos los detectados.
CASE ?=
ifeq ($(CASE),)
  CASES := $(ALL_CASES)
else
  CASES := $(CASE)
endif

.PHONY: all help list build experiments clean $(CASES)

all: help

help: ## Show available targets
	@echo "Available targets:"
	@grep -E '^[a-zA-Z0-9_%./-]+:.*## ' $(MAKEFILE_LIST) | \
	awk 'BEGIN {FS = ":.*## "}; {printf "  %-20s %s\n", $$1, $$2}'
	@echo ""
	@echo "Casos detectados: $(ALL_CASES)"
	@echo "Ejemplos:"
	@echo "  make build"
	@echo "  make build CASE=case_1"
	@echo "  make experiments CASE=case_1 N=1024 BLOCK_SIZE=128 THREADS=8 ITER=200"

list: ## List detected case_* directories (each with its own Makefile)
	@echo "$(ALL_CASES)"

build: ## Build every case (or only CASE=case_X) — delegates to each case's own Makefile/compiler
	@for case in $(CASES); do \
	  echo "== build: $$case =="; \
	  $(MAKE) -C $$case build $(MAKEOVERRIDES) || exit 1; \
	done

experiments: ## Run the >=200-iteration experiments of every case (or CASE=case_X)
	@for case in $(CASES); do \
	  echo "== experiments: $$case =="; \
	  $(MAKE) -C $$case experiments $(MAKEOVERRIDES) || exit 1; \
	done

clean: ## Clean build/ and results/ of every case (or CASE=case_X)
	@for case in $(CASES); do \
	  echo "== clean: $$case =="; \
	  $(MAKE) -C $$case clean; \
	done
