//# exit: 1
//# stderr: tests/bad/expr/logical-non-scalar.c:9:12: error: operands must have scalar types
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    return pair && 1;
}
