//# stderr: tests/bad/structs/member-on-non-struct.c:4:12: error: member access on non-struct
int main() {
    int value;
    return value.other;
}
