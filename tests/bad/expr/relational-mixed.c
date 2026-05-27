//# exit: 1
//# stderr: tests/bad/expr/relational-mixed.c:6:12: error: operands must both be integers or pointers
int main() {
    int *ptr;
    int value;
    return ptr < value;
}
