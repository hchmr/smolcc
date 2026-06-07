//# stderr: tests/bad/decl/conflict-different-kind.c:6:5: error: 'value' already declared as a different kind of symbol. Previous declaration at tests/bad/decl/conflict-different-kind.c:3:5
enum {
    value,
};

int value;
