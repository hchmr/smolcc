//# mode: run
//# exit: 9

struct Pair {
    int x;
    int y;
};

int main() {
    struct Pair a;
    a.x = 1;
    a.y = 9;
    return ((a.x = 1), a).y;
}
