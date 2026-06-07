//# stderr: tests/bad/structs/member-on-incomplete.c:6:12: error: member access on incomplete struct
struct Pair;

int main() {
    struct Pair *pair;
    return pair->value;
}
