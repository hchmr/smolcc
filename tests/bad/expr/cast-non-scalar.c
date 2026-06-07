//# stderr: tests/bad/expr/cast-non-scalar.c:8:12: error: cast requires scalar
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    return (int)pair;
}
