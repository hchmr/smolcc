//# mode: run
//# exit: 5

struct point {
    int x;
    int y;
};

int main() {
    struct point p;
    p.x = 5;
    p.y = 6;
    struct point *pp = &p;
    *pp;  // no-op
    return p.x;
}
