//# stderr: tests/bad/expr/conditional-incompatible.c:5:16: error: incompatible pointer operands
int main() {
    int *lhs;
    char *rhs;
    return 0 ? lhs : rhs;
}
