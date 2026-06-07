//# stderr: tests/bad/call/void-pointer-arg-from-int.c:6:17: error: target type mismatch

int sink(void *p);

int f() {
    return sink(1);
}
