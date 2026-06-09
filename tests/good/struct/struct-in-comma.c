//# mode: run
//# exit: 9

struct point {
    int x;
    int y;
};

static int side_effect;

static int compute() {
    side_effect = 1;
    return 9;
}

int main() {
    struct point p;
    p.x = 3;
    (p, compute());  // struct in comma first position, result is int
    return side_effect ? 9 : 0;
}
