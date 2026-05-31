//# mode: run
//# exit: 2

struct Pair {
    int x;
    int y;
};

int main() {
    struct Pair a;
    struct Pair b;
    b.x = 1;
    b.y = 2;
    return (a = b).y;
}

