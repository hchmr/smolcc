//# exit: 1
//# stderr: tests/bad/control/return-value-from-void.c:4:12: error: expected ';'
void fail() {
    return 1;
}
