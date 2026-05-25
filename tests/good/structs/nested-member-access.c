//# mode: run
//# exit: 24

struct Point {
    int x;
    int y;
};

struct Rect {
    struct Point top_left;
    struct Point bottom_right;
};

int main() {
    struct Rect rect;
    rect.top_left.x = 4;
    rect.top_left.y = 6;
    rect.bottom_right.x = 10;
    rect.bottom_right.y = 2;
    return (rect.bottom_right.x - rect.top_left.x) * (rect.top_left.y - rect.bottom_right.y);
}