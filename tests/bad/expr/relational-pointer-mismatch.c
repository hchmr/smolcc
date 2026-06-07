//# stderr: tests/bad/expr/relational-pointer-mismatch.c:5:12: error: pointer types must match
int main() {
    int *lhs;
    char *rhs;
    return lhs < rhs;
}
