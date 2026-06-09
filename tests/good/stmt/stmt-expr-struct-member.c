//# description: verify that struct member access can be used as a statement expression
//# mode: run

struct box {
    int v;
} box;

int main() {
    box.v;  // no-op
    return 0;
}
