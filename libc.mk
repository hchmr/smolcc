OUT_DIR = out/libc
SRC_DIR = libc
SMOLLIBC = $(OUT_DIR)/libc.a
SRCS = $(shell find $(SRC_DIR) -name '*.c')
ASMS = $(shell find $(SRC_DIR) -name '*.s')
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.o) \
	   $(ASMS:$(SRC_DIR)/%.s=$(OUT_DIR)/%.o)
CC ?= cc
HOST_CC ?= 0
ifneq ($(HOST_CC), 0)
	COMPILE_C = $(CC)
else
	COMPILE_C = SMOLCC=./out/2/smolcc ./scripts/compile
endif


CFLAGS = -g -ffreestanding -fno-builtin \
	-std=c99 \
	-Wall -Wextra -Wpedantic \
	-Wno-strict-prototypes -Wno-parentheses -Wno-return-type -Wno-empty-body
LDFLAGS = -nostdlib

all: $(SMOLLIBC)

$(SMOLLIBC): $(OBJS)
	rm -f $@
	ar rcs $@ $^

$(OUT_DIR)/%.o: $(SRC_DIR)/%.c | $(OUT_DIR)
	$(COMPILE_C) -c -o $@ $<

$(OUT_DIR)/%.o: $(SRC_DIR)/%.s | $(OUT_DIR)
	$(CC) -c -o $@ $<

$(OUT_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(OUT_DIR)
