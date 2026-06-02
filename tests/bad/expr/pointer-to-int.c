//# mode: compile-only
//# exit: 1
//# stderr: tests/bad/expr/pointer-to-int.c:7:17: error: target type mismatch

int main() {
    int xs[1];
    int value = xs;
    return value;
}
