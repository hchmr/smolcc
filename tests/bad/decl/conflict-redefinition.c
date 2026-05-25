//# exit: 1
//# stderr: tests/bad/decl/conflict-redefinition.c:4:5: error: 'value' already defined. Previous declaration at tests/bad/decl/conflict-redefinition.c:3:5
int value = 1;
int value = 2;
