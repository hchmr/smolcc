//# exit: 1
//# stderr: tests/bad/expr/address-not-addressable.c:4:12: error: operand must be addressable
int main() {
    return &(1);
}
