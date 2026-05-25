//# mode: run
//# exit: 34

int main() {
    int xs[2][3];
    xs[0][0] = 1;
    xs[0][1] = 2;
    xs[0][2] = 3;
    xs[1][0] = 4;
    xs[1][1] = 5;
    xs[1][2] = 6;
    return xs[0][2] * 10 + xs[1][0];
}
