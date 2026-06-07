//# mode: run
//# exit: 20

struct Pair {
    char tag;
    int value;
};

int main() {
    return sizeof(int[3]) + sizeof(struct Pair);
}
