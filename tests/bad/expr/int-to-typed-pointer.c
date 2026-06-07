//# mode: compile-only
//# stderr: tests/bad/expr/int-to-typed-pointer.c:6:9: error: target type mismatch

int main() {
    int *p;
    p = 1;
    return 0;
}
