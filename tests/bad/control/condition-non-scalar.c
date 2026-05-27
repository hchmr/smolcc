//# exit: 1
//# stderr: tests/bad/control/condition-non-scalar.c:9:9: error: condition must be scalar
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    if (pair)
        return 1;
    return 0;
}
