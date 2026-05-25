//# exit: 1
//# stderr: tests/bad/expr/incompatible-pointer-operands.c:6:12: error: incompatible pointer operands
int main() {
    int *lhs;
    char *rhs;
    return lhs == rhs;
}
