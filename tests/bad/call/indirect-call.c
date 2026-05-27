//# exit: 1
//# stderr: tests/bad/call/indirect-call.c:4:12: error: indirect calls are not supported
int apply(int func(int)) {
    return func(1);
}
