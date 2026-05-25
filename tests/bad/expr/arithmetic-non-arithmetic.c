//# exit: 1
//# stderr: tests/bad/expr/arithmetic-non-arithmetic.c:9:12: error: operands must have arithmetic types
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    return pair + 1;
}
