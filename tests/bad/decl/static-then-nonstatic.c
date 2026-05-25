//# exit: 1
//# stderr: tests/bad/decl/static-then-nonstatic.c:5:5: error: 'value' already declared as static. Previous declaration at tests/bad/decl/static-then-nonstatic.c:4:12

static int value;  // tentative definition, internal linkage
int value;  // redeclaration with external linkage