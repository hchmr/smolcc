//# exit: 1
//# stderr: tests/bad/expr/shift-non-integer.c:5:12: error: operands must be integers
int main() {
    int *ptr;
    return ptr << 1;
}
