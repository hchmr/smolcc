//# mode: run
//# stdout: 4294967295
//# stdout: 0
//# stdout: 10000000000

extern int printf(const char *fmt, ...);

unsigned int uadd(unsigned int a, unsigned int b) {
    return a + b;
}

unsigned long uladd(unsigned long a, unsigned long b) {
    return a + b;
}

int main() {
    // add without wrapping
    unsigned int result1 = uadd(4294967290U, 5U);
    printf("%u\n", result1);

    // add with wrapping
    unsigned int result2 = uadd(4294967295U, 1U);
    printf("%u\n", result2);

    // unsigned long addition without wrapping
    unsigned long result3 = uladd(5000000000UL, 5000000000UL);
    printf("%lu\n", result3);

    return 0;
}
