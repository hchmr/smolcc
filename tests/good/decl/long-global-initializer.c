//# mode: run
//# stdout: 1099511627776

extern int printf(const char *fmt, ...);

long big = (long)1 << 40;

int main() {
    printf("%ld\n", big);
    return 0;
}
