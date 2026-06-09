//# mode: run
//# stdout: 2147483647
//# stdout: 1
//# stdout: 4
//# stdout: 1

extern int printf(const char *fmt, ...);

int main() {
    // Unsigned division (udiv) vs signed division (sdiv)
    // Key: UINT_MAX / 2 = 2147483647 (not 0 like signed -1 / 2)
    unsigned int u_max = 4294967295U;
    printf("%u\n", u_max / 2U);
    printf("%u\n", u_max % 2U);

    // Regular unsigned division and modulo
    unsigned int x = 21U;
    unsigned int y = 5U;
    printf("%u\n", x / y);
    printf("%u\n", x % y);

    return 0;
}
