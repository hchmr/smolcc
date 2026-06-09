//# mode: run
//# exit: 9

struct Pair {
    int left;
    int right;
};

void assign(struct Pair *p, int left, int right) {
    p->left = left;
    p->right = right;
}

int main() {
    struct Pair pair;
    assign(&pair, 4, 5);
    return pair.left + pair.right;
}
