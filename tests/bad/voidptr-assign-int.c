//# exit: 1
//# stderr: tests/bad/voidptr-assign-int.c:6:6: error: target type mismatch. Pointer types are incompatible.
int f() {
	void *p;

	p = 1;
	return 0;
}
