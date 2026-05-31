STAGE ?= stage2
ARGS ?=

### Outputs

export BUILD_DIR ?= build
BIN_DIR ?= $(BUILD_DIR)/bin
LIB_DIR ?= $(BUILD_DIR)/lib

### Inputs

MK_DIR = mk
STAGE_MK = $(MK_DIR)/stage.mk
STDLIB_MK = $(MK_DIR)/stdlib.mk

### Rules

.PHONY: all compiler bootstrap stdlib clean

all: compiler stdlib

clean:
	rm -rf $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $@

$(LIB_DIR):
	mkdir -p $@

stage0:
	@$(MAKE) -f $(STAGE_MK) STAGE=stage0

stage1: stage0
	@$(MAKE) -f $(STAGE_MK) STAGE=stage1

stage2: stage1
	@$(MAKE) -f $(STAGE_MK) STAGE=stage2
	@printf "\n===> %s\n\n" "Comparing stage1 and stage2 outputs..."
	diff -u $(BUILD_DIR)/stage1/smolcc1.s $(BUILD_DIR)/stage2/smolcc1.s
	@echo "stage1 and stage2 outputs are identical\n"

bootstrap: $(STAGE)

compiler: bootstrap | $(BIN_DIR) ./scripts/install
	./scripts/install -s $(STAGE) $(BIN_DIR)

stdlib: compiler | $(LIB_DIR)
	$(MAKE) -f $(STDLIB_MK)
	cp $(BUILD_DIR)/stdlib/libc.a $(LIB_DIR)/libc.a
	cp $(BUILD_DIR)/stdlib/crt.o $(LIB_DIR)/crt.o

test: all
	./scripts/test --no-build $(ARGS)
