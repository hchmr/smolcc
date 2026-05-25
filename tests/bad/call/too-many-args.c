//# exit: 1
//# stderr: tests/bad/call/too-many-args.c:6:12: error: too many arguments in function call
int add(int value);

int main() {
    return add(1, 2);
}
