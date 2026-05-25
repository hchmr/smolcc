//# exit: 1
//# stderr: tests/bad/decl/nonstatic-then-static.c:5:12: error: 'value' already declared as non-static. Previous declaration at tests/bad/decl/nonstatic-then-static.c:4:5

int value;  // tentative definition, external linkage
static int value;  // redeclaration with internal linkage