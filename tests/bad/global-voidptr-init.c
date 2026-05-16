//# exit: 1
//# stderr: tests/bad/global-voidptr-init.c:3:11: error: target type mismatch
void *p = 0 - 0;

int main() {
    return 0;
}
