### Inputs

SRC_DIR = stdlib
SRCS = $(shell find $(SRC_DIR) -name '*.c')
ASMS = $(shell find $(SRC_DIR) -name '*.s')
CRT_ASMS = $(filter $(SRC_DIR)/crt%.s, $(ASMS))
LIBC_ASMS = $(filter-out $(CRT_ASMS), $(ASMS))

### Outputs

BUILD_DIR ?= build
OUT_DIR = $(BUILD_DIR)/stdlib
OBJ_DIR = $(OUT_DIR)/obj

CRT_OBJS = $(CRT_ASMS:$(SRC_DIR)/%.s=$(OBJ_DIR)/%.o)
LIBC_OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) \
			$(LIBC_ASMS:$(SRC_DIR)/%.s=$(OBJ_DIR)/%.o)

LIBC = $(OUT_DIR)/libc.a
CRT = $(OUT_DIR)/crt.o

### Tools

SMOLCC ?= $(BUILD_DIR)/bin/smolcc
CC = cc
ASFLAGS = -g
LDFLAGS = -nostdlib

### Rules

.PHONY: all clean

all: $(LIBC) $(CRT)

clean:
	rm -rf $(OUT_DIR)

$(OUT_DIR):
	mkdir -p $@

$(OBJ_DIR): | $(OUT_DIR)
	mkdir -p $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(SMOLCC) -c -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s | $(OBJ_DIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

$(LIBC): $(LIBC_OBJS)
	ar rcs $@ $^

$(CRT): $(CRT_OBJS) | $(OUT_DIR)
	$(CC) $(LDFLAGS) -r -o $@ $^
