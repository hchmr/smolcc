//# mode: run
//# exit: 0
//# stdout: 4294967295
//# stdout: 18446744073709551615
//# stdout: 4294967040
//# stdout: 255
//# stdout: 2863267840

extern int printf(const char *fmt, ...);

int main() {
    // Bitwise NOT on unsigned int: ~0U must give UINT_MAX
    unsigned int u0 = 0U;
    printf("%u\n", ~u0);

    // Bitwise NOT on unsigned long: ~0UL must give ULONG_MAX
    unsigned long ul0 = 0UL;
    printf("%lu\n", ~ul0);

    // Bitwise AND
    unsigned int a = 4294967295U;
    unsigned int b = 4294967040U;
    printf("%u\n", a & b);

    // Bitwise OR
    unsigned int c = 240U;
    unsigned int d = 15U;
    printf("%u\n", c | d);

    // Bitwise XOR (identity: v ^ x ^ x = v)
    unsigned int v = 2863267840U;
    unsigned int x = 2863267840U;
    printf("%u\n", (v ^ x) ^ x);

    return 0;
}
