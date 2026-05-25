//# exit: 1
//# stderr: tests/bad/decl/conflict-different-kind.c:7:5: error: 'value' already declared as a different kind of symbol. Previous declaration at tests/bad/decl/conflict-different-kind.c:4:5
enum {
    value,
};

int value;
