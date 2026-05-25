//# exit: 1
//# stderr: tests/bad/call/indirect-call.c:4:12: error: only direct function calls are supported
int apply(int func(int)) {
    return func(1);
}
