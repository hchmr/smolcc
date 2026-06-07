//# stderr: tests/bad/call/too-many-args.c:5:12: error: wrong number of arguments
int add(int value);

int main() {
    return add(1, 2);
}
