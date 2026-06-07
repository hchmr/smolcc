//# mode: run
//# exit: 1

enum {
    Value = (char)255,
};

int main() {
    return Value < 0;
}
