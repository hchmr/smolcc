//# stderr: tests/bad/expr/shift-non-integer.c:4:12: error: operands must be integers
int main() {
    int *ptr;
    return ptr << 1;
}
