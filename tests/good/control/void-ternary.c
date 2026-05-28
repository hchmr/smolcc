//# mode: run
//# exit: 2

static int result;

static void set_a() {
    result = 1;
}
static void set_b() {
    result = 2;
}

int main() {
    int x = 0;
    x ? set_a() : set_b();
    return result;
}
