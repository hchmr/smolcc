//# exit: 1
//# stderr: tests/bad/call/called-object-not-function.c:6:12: error: called object is not a function
int value;

int main() {
    return value();
}
