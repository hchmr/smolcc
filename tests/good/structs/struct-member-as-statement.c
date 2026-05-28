//# mode: run
//# exit: 7

struct inner {
    int v;
};
struct outer {
    struct inner i;
    int extra;
};

int main() {
    struct outer o;
    o.i.v = 7;
    o.extra = 99;
    o.i;  // struct-typed member access as statement expression
    return o.i.v;
}
