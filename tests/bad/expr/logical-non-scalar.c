//# stderr: tests/bad/expr/logical-non-scalar.c:8:12: error: operands must be scalar
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    return pair && 1;
}
