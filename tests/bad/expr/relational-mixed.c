//# stderr: tests/bad/expr/relational-mixed.c:5:12: error: operands must both be integers or pointers
int main() {
    int *ptr;
    int value;
    return ptr < value;
}
