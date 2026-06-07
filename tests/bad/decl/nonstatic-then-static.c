//# stderr: tests/bad/decl/nonstatic-then-static.c:4:12: error: 'value' already declared as non-static. Previous declaration at tests/bad/decl/nonstatic-then-static.c:3:5

int value;  // tentative definition, external linkage
static int value;  // redeclaration with internal linkage
