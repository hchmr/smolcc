//# stderr: tests/bad/expr/void-pointer-assign-from-int.c:5:9: error: target type mismatch

int f() {
    void *p;
    p = 1;
    return 0;
}
