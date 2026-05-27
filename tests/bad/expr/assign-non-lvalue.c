//# exit: 1
//# stderr: tests/bad/expr/assign-non-lvalue.c:4:5: error: operand not assignable
int main() {
    1 = 2;
    return 0;
}
