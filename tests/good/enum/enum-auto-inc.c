//# description: Verify that enumerators automatically increment from the previous value
//# mode: run
//# exit: 9

enum {
    A = 4,
    B,
};

int main() {
    return A + B;
}
