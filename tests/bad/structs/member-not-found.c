//# exit: 1
//# stderr: tests/bad/structs/member-not-found.c:9:12: error: no such member
struct Pair {
    int value;
};

int main() {
    struct Pair pair;
    return pair.other;
}
