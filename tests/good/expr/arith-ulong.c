//# mode: run
//# stdout: 0
//# stdout: 18446744073709551615
//# stdout: 9223372036854775807
//# stdout: 9223372036854775807
//# stdout: 1

extern int printf(const char *fmt, ...);

int main() {
    // Unsigned long overflow: ULONG_MAX + 1 = 0
    unsigned long ul_max = 18446744073709551615UL;
    printf("%lu\n", ul_max + 1UL);

    // Unsigned long underflow: 0 - 1 = ULONG_MAX
    unsigned long zero = 0UL;
    printf("%lu\n", zero - 1UL);

    // Unsigned long division
    printf("%lu\n", ul_max / 2UL);

    // Unsigned long right shift (lsr fills with 0, not sign bit)
    printf("%lu\n", ul_max >> 1);

    // Unsigned long modulo
    printf("%lu\n", ul_max % 7UL);

    return 0;
}
