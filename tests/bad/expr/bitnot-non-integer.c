//# stderr: tests/bad/expr/bitnot-non-integer.c:4:12: error: operand must be integer
int main() {
    int *ptr;
    return ~ptr;
}
