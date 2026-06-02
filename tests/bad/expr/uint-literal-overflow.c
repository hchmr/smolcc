//# mode: compile-only
//# exit: 1
//# stderr: tests/bad/expr/uint-literal-overflow.c:5:22: error: integer literal overflow

unsigned int value = 4294967296U;
