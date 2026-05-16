//# exit: 1
//# stderr: tests/bad/global-static-missing.c:4:5: error: 'x' already declared as static. Previous declaration at tests/bad/global-static-missing.c:3:12
static int x;
int x;
