//# exit: 1
//# stderr: tests/bad/expr/bitnot-non-integer.c:5:12: error: operand must be integer
int main() {
    int *ptr;
    return ~ptr;
}
