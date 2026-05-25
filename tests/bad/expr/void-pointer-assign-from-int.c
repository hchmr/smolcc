//# exit: 1
//# stderr: tests/bad/expr/void-pointer-assign-from-int.c:6:9: error: target type mismatch

int f() {
    void *p;
    p = 1;
    return 0;
}