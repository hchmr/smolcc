//# mode: compile-only
//# exit: 1
//# stderr: tests/bad/expr/mismatched-pointer-assign.c:7:15: error: target type mismatch

int main() {
    int value;
    long *p = &value;
    return 0;
}
