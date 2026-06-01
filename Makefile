STAGE ?= stage2
ARGS ?=
PREFIX ?=

### Output directories

export BUILD_DIR ?= build
DIST_DIR ?= $(BUILD_DIR)/dist
BOOTSTRAP_DIR = $(BUILD_DIR)/bootstrap

### Makefiles

MK_DIR = mk
STAGE_MK = $(MK_DIR)/stage.mk
STDLIB_MK = $(MK_DIR)/stdlib.mk

### Rules

.PHONY: all clean stage0 stage1 stage2 bootstrap compiler stdlib test install

all: compiler stdlib

clean:
	rm -rf $(BUILD_DIR)

$(DIST_DIR):
	mkdir -p $@

stage0: $(STAGE_MK)
	$(MAKE) -f $(STAGE_MK) STAGE=stage0

stage1: stage0
	$(MAKE) -f $(STAGE_MK) STAGE=stage1

stage2: stage1
	$(MAKE) -f $(STAGE_MK) STAGE=stage2
	@printf "\n===> %s\n\n" "Comparing stage1 and stage2 outputs..."
	diff -u $(BOOTSTRAP_DIR)/stage1/smolcc1.s $(BOOTSTRAP_DIR)/stage2/smolcc1.s
	@echo "stage1 and stage2 outputs are identical\n"

bootstrap: $(STAGE)

compiler: bootstrap | $(DIST_DIR)
	./scripts/install --component bin --stage $(STAGE) --prefix $(DIST_DIR)

stdlib: compiler $(STDLIB_MK)
	$(MAKE) -f $(STDLIB_MK) SMOLCC=$(DIST_DIR)/bin/smolcc
	./scripts/install --component lib --stage $(STAGE) --prefix $(DIST_DIR)

test: all
	./scripts/test --no-build $(ARGS)

install: all
	@test -n "$(PREFIX)" || { echo "Error: PREFIX is not set" >&2; exit 1; }
	mkdir -p $(PREFIX)
	./scripts/install --component all --stage $(STAGE) --prefix $(PREFIX)
