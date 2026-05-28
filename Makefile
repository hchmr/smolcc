OUT_DIR = out/0
SMOLCC = $(OUT_DIR)/smolcc
SRCS = main.c
ASMS = sys.s
OBJS = $(SRCS:%.c=$(OUT_DIR)/%.o) $(ASMS:%.s=$(OUT_DIR)/%.o)
CC = cc
CFLAGS = -g -ffreestanding -fno-builtin \
	-std=c99 \
	-Wall -Wextra -Wpedantic \
	-Wno-strict-prototypes -Wno-parentheses -Wno-return-type -Wno-empty-body
LDFLAGS = -nostdlib

all: $(SMOLCC)

$(SMOLCC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OUT_DIR)/%.o: %.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_DIR)/%.o: %.s | $(OUT_DIR)
	$(CC) -c -o $@ $<

$(OUT_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(OUT_DIR)