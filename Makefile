OUT_DIR = out
TARGET = $(OUT_DIR)/ucc
SRCS = main.c
OBJS = $(SRCS:%.c=$(OUT_DIR)/%.o)
CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 \
	-ffreestanding -fno-stack-protector -fno-builtin \
	-Wno-strict-prototypes -Wno-logical-op-parentheses
LDFLAGS = -nostdlib
LDLIBS = -lSystem

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OUT_DIR)/%.o: %.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(OUT_DIR)
