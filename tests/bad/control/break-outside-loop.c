//# exit: 1
//# stderr: tests/bad/control/break-outside-loop.c:4:5: error: break/continue outside loop
int main() {
    break;
    return 0;
}
