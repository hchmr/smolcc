//# mode: run
//# exit: 2
//# desription: pointer subtraction is measured in elements, not bytes.

int main() {
    int xs[4];
    return &xs[3] - &xs[1];
}