//# exit: 1
//# stderr: tests/bad/control/break-outside-loop.c:4:5: error: break/continue statement outside loop
int main() {
    break;
    return 0;
}
