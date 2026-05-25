//# exit: 1
//# stderr: tests/bad/expr/cast-non-scalar.c:9:12: error: cast requires scalar types
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    return (int)pair;
}
