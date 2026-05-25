//# exit: 1
//# stderr: tests/bad/structs/member-on-non-struct.c:5:12: error: member access on non-struct type
int main() {
    int value;
    return value.other;
}
