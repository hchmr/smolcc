//# stderr: tests/bad/expr/bitwise-non-integer.c:4:12: error: operands must have integer types
int main() {
    int *ptr;
    return ptr & 1;
}
