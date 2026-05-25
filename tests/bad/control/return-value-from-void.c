//# exit: 1
//# stderr: tests/bad/control/return-value-from-void.c:4:5: error: returning a value from a void function
void fail() {
    return 1;
}
