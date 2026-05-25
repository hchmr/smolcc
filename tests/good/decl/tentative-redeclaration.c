//# mode: compile-only
//# exit: 0

int value;  // tentative definition, external linkage
int value;  // same tentative definition
extern int value;  // refers to previous
