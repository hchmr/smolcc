//# mode: run
//# exit: 0
//# stdout: x=42 y=7

extern int printf(const char *format, ...);

int main() {
    printf("x=%d y=%d\n", 42, 7);
    return 0;
}