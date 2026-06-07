//# stderr: tests/bad/expr/assign-non-lvalue.c:3:5: error: operand not assignable
int main() {
    1 = 2;
    return 0;
}
