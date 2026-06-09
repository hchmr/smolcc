//# mode: run
//# exit: 8

struct Pair {
    char tag;
    int value;
};

int main() {
    return sizeof(struct Pair);
}
