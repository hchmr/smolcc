//# description: verify that a struct can be used as a statement expression
//# mode: run
//# exit: 3

struct point {
    int x;
    int y;
};

int main() {
    struct point p;
    p.x = 3;
    p.y = 4;
    p;  // aggregate lvalue as statement expression
    return p.x;
}
