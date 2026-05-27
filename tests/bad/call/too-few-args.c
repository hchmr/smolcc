//# exit: 1
//# stderr: tests/bad/call/too-few-args.c:6:12: error: too few arguments
int add(int lhs, int rhs);

int main() {
    return add(1);
}
