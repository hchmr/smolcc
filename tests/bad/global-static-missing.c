//# exit: 1
//# stderr: tests/bad/global-static-missing.c:5:5: error: 'x' already declared as static. Previous declaration at tests/bad/global-static-missing.c:4:12

static int x;
int x;
