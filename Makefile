OUT_DIR = out
TARGET = $(OUT_DIR)/ucc
SRCS = main.c
OBJS = $(SRCS:%.c=$(OUT_DIR)/%.o)
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -Wconversion

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

$(OUT_DIR)/%.o: %.c | $(OUT_DIR)
	$(CC) -c -o $@ $<

$(OUT_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(OUT_DIR)
