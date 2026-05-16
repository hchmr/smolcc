//# mode: run
//# exit: 24
struct point {
    int x;
    int y;
};

struct rect {
    struct point top_left;
    struct point bottom_right;
};

void init_point(struct point *p, int x, int y) {
    p->x = x;
    p->y = y;
}

void init_rect(struct rect *r, struct point *top_left, struct point *bottom_right) {
    r->top_left = *top_left;
    r->bottom_right = *bottom_right;
}

int abs(int n) {
    return n < 0 ? -n : n;
}

int area(struct rect *r) {
    int width = abs(r->bottom_right.x - r->top_left.x);
    int height = abs(r->bottom_right.y - r->top_left.y);
    return width * height;
}

int main() {
    struct point p1, p2;
    struct rect r;

    init_point(&p1, 10, 2);
    init_point(&p2, 4, 6);
    init_rect(&r, &p1, &p2);
    int area_val = area(&r);
    return area_val;
}
