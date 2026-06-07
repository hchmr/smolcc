//# mode: run
//# exit: 9

struct Pair {
    int left;
    int right;
};

int main() {
    struct Pair pair;
    pair.left = 4;
    pair.right = 5;
    return pair.left + pair.right;
}
