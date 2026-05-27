//# exit: 1
//# stderr: tests/bad/expr/postinc-bad-type.c:9:5: error: operand cannot be incremented/decremented
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    pair++;
    return 0;
}
