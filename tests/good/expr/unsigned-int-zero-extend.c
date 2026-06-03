//# mode: run
//# exit: 0
//# stdout: 4294967295
//# stdout: 1
//# stdout: -1
//# stdout: 0

extern int printf(const char *fmt, ...);

int main() {
    // Casting unsigned int to long must zero-extend (uxtw)
    unsigned int u = 4294967295U;
    long lu = (long)u;
    printf("%ld\n", lu);

    // Result should be positive (> 0)
    printf("%d\n", lu > 0);

    // Casting signed int to long must sign-extend (sxtw)
    int s = -1;
    long ls = (long)s;
    printf("%ld\n", ls);

    // Result should be negative (< 0)
    printf("%d\n", ls > 0);

    return 0;
}
