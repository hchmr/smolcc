//# exit: 1
//# stderr: tests/bad/voidptr-arg-int.c:6:17: error: target type mismatch. Pointer types are incompatible.
int sink(void *p);

int f() {
    return sink(1);
}
