//# stderr: tests/bad/expr/address-not-addressable.c:3:12: error: operand not addressable
int main() {
    return &(1);
}
