//# mode: run
//# stdout: 0
//# stdout: 256
//# stdout: 2147483648
//# stdout: 4294967296

extern int printf(const char *fmt, ...);

int main() {
    char c = -1;
    printf("%d\n", c + 1);

    unsigned char uc = 255;
    printf("%d\n", uc + 1);

    int i = 2147483647;
    printf("%ld\n", i + 1L);

    unsigned int u = 4294967295U;
    printf("%ld\n", u + 1L);

    return 0;
}
