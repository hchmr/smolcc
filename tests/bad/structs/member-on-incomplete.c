//# exit: 1
//# stderr: tests/bad/structs/member-on-incomplete.c:7:12: error: member access on incomplete struct
struct Pair;

int main() {
    struct Pair *pair;
    return pair->value;
}
