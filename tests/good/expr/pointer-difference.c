//# description: pointer subtraction is measured in elements, not bytes.
//# mode: run
//# exit: 2

int main() {
    int xs[4];
    return &xs[3] - &xs[1];
}
