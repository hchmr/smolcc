//# stderr: tests/bad/expr/neg-non-arithmetic.c:8:12: error: operand must be integer
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    return -pair;
}
