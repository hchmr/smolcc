//# stderr: tests/bad/expr/incompatible-pointer-operands.c:5:12: error: incompatible pointer operands
int main() {
    int *lhs;
    char *rhs;
    return lhs == rhs;
}
