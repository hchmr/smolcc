//# mode: compile-only
//# exit: 1
//# stderr: tests/bad/expr/int-to-typed-pointer.c:7:9: error: target type mismatch

int main() {
    int *p;
    p = 1;
    return 0;
}
