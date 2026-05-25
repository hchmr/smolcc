//# mode: run
//# exit: 6
//# desription: array parameters are adjusted to pointers, so indexing still works through the parameter.

int pick_last(int xs[3]) {
    return xs[2];
}

int main() {
    int xs[3];
    xs[0] = 4;
    xs[1] = 5;
    xs[2] = 6;
    return pick_last(xs);
}