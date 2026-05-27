//# exit: 1
//# stderr: tests/bad/call/too-many-args.c:6:12: error: too many arguments
int add(int value);

int main() {
    return add(1, 2);
}
