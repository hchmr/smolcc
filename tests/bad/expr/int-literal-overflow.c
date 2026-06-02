//# mode: compile-only
//# exit: 1
//# stderr: tests/bad/expr/int-literal-overflow.c:5:13: error: integer literal overflow

int value = 2147483648;
