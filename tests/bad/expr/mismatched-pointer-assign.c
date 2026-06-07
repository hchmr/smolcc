//# mode: compile-only
//# stderr: tests/bad/expr/mismatched-pointer-assign.c:6:15: error: target type mismatch

int main() {
    int value;
    long *p = &value;
    return 0;
}
