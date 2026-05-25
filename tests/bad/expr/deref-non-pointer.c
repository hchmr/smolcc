//# exit: 1
//# stderr: tests/bad/expr/deref-non-pointer.c:4:12: error: operand must be a pointer
int main() {
    return *1;
}
