OUT_DIR = out/0
SRC_DIR = src
SMOLCC = $(OUT_DIR)/smolcc
SRCS = $(SRC_DIR)/smolcc.c
ASMS = $(SRC_DIR)/sys.s
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OUT_DIR)/%.o) \
	   $(ASMS:$(SRC_DIR)/%.s=$(OUT_DIR)/%.o)
CC = cc
CFLAGS = -g -ffreestanding -fno-builtin \
	-std=c99 \
	-Wall -Wextra -Wpedantic \
	-Wno-strict-prototypes -Wno-parentheses -Wno-return-type -Wno-empty-body
LDFLAGS = -nostdlib

all: $(SMOLCC)

$(SMOLCC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OUT_DIR)/%.o: $(SRC_DIR)/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_DIR)/%.o: $(SRC_DIR)/%.s | $(OUT_DIR)
	$(CC) -c -o $@ $<

$(OUT_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(OUT_DIR)