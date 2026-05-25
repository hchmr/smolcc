//# exit: 1
//# stderr: tests/bad/expr/bitwise-non-integer.c:5:12: error: operands must have integer types
int main() {
    int *ptr;
    return ptr & 1;
}
