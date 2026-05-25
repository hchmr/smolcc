//# exit: 1
//# stderr: tests/bad/expr/neg-non-arithmetic.c:9:12: error: operand must be arithmetic
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    return -pair;
}
