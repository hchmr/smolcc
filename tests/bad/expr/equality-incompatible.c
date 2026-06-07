//# stderr: tests/bad/expr/equality-incompatible.c:5:12: error: operands must have compatible types
int main() {
    int *ptr;
    int value;
    return ptr == value;
}
