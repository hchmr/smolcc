//# mode: run
//# exit: 7

struct Pair {
    int left;
    int right;
};

int main() {
    struct Pair src;
    struct Pair dst;
    src.left = 3;
    src.right = 4;
    dst = src;
    return dst.left + dst.right;
}