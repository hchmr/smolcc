//# exit: 1
//# stderr: tests/bad/expr/postinc-bad-type.c:9:5: error: cannot increment/decrement operand of this type
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    pair++;
    return 0;
}
