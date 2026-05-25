//# exit: 1
//# stderr: tests/bad/expr/relational-mixed.c:6:12: error: operands must be both integers or both pointers
int main() {
    int *ptr;
    int value;
    return ptr < value;
}
