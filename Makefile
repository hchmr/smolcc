OUT_DIR = out
MINICC = $(OUT_DIR)/minicc
SRCS = main.c
OBJS = $(SRCS:%.c=$(OUT_DIR)/%.o)
CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 \
	-Wno-strict-prototypes -Wno-parentheses -Wno-return-type -Wno-empty-body -g

all: $(MINICC)

$(MINICC): $(OBJS)
	$(CC) -o $@ $^

$(OUT_DIR)/%.o: %.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(OUT_DIR)
