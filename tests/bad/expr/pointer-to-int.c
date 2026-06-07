//# mode: compile-only
//# stderr: tests/bad/expr/pointer-to-int.c:6:17: error: target type mismatch

int main() {
    int xs[1];
    int value = xs;
    return value;
}
