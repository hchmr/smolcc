//# exit: 1
//# stderr: tests/bad/expr/ptrdiff-mismatch.c:6:12: error: pointer types must match
int main() {
    int *lhs;
    char *rhs;
    return lhs - rhs;
}
