//# mode: run
//# exit: 0
//# stdout: x=420 y=71234

extern int printf(const char *format, ...);

int main() {
    printf("x=%d y=%d\n", 420, 71234);
    return 0;
}
