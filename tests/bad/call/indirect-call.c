//# stderr: tests/bad/call/indirect-call.c:3:12: error: indirect calls are not supported
int apply(int func(int)) {
    return func(1);
}
