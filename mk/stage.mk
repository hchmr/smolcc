STAGE ?= stage0

### Stage numbering

ifneq ($(STAGE),stage0)
STAGE_NUMBER = $(STAGE:stage%=%)
PREV_STAGE_NUMBER = $(shell echo "$$(($(STAGE_NUMBER) - 1))")
PREV_STAGE = stage$(PREV_STAGE_NUMBER)
endif

### Inputs

SRC_DIR = compiler
SRCS = $(shell find $(SRC_DIR) -name '*.c')
ASMS = $(shell find $(SRC_DIR) -name '*.s')

### Outputs

BUILD_DIR ?= build
OUT_DIR = $(BUILD_DIR)/bootstrap/$(STAGE)
COMPILER = $(OUT_DIR)/smolcc1

ifeq ($(STAGE),stage0)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.o) \
	   $(ASMS:$(SRC_DIR)/%.s=$(OUT_DIR)/%.o)
else
ASM_OUTS = $(SRCS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.s)
.SECONDARY: $(ASM_OUTS)

OBJS = $(ASM_OUTS:.s=.o) \
	   $(ASMS:$(SRC_DIR)/%.s=$(OUT_DIR)/%.o)
endif

### Tools

CC ?= cc
ASFLAGS = -g
LDFLAGS = -nostdlib

ifeq ($(STAGE),stage0)
CFLAGS = \
	-g \
	-ffreestanding -fno-builtin \
	-std=c99 \
	-Wall -Wextra -Wpedantic \
	-Wno-strict-prototypes -Wno-parentheses -Wno-return-type -Wno-empty-body
else
SMOLCC1 = $(BUILD_DIR)/bootstrap/$(PREV_STAGE)/smolcc1
endif

### Rules

.PHONY: all clean print_header

all: print_header $(COMPILER)

clean:
	rm -rf $(OUT_DIR)

$(OUT_DIR):
	mkdir -p $@

ifeq ($(STAGE),stage0)
# Use host compiler to compile stage0
$(OUT_DIR)/%.o: $(SRC_DIR)/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<
else
# Use previous stage to compile this stage
$(OUT_DIR)/%.s: $(SRC_DIR)/%.c | $(OUT_DIR)
	$(SMOLCC1) < $< | ./scripts/asm-fmt > $@

$(OUT_DIR)/%.o: $(OUT_DIR)/%.s | $(OUT_DIR)
	$(CC) $(ASFLAGS) -c -o $@ $<
endif

$(OUT_DIR)/%.o: $(SRC_DIR)/%.s | $(OUT_DIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

$(COMPILER): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

print_header:
	@printf "\n===> %s\n\n" "Building $(STAGE)..."
