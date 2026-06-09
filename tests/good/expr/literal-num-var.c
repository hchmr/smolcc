//# description: Verify that large integer literals are correctly loaded
//# mode: run

extern int printf(const char *fmt, ...);

int main() {
    int i_max = 2147483647;
    int i_min = -2147483648;
    unsigned int u_max = 4294967295U;
    long l_max = 9223372036854775807L;
    long l_min = -9223372036854775807L - 1;
    unsigned long ul_max = 18446744073709551615UL;

    printf("%x\n", i_max);
    printf("%x\n", i_min);
    printf("%x\n", u_max);
    printf("%lx\n", l_max);
    printf("%lx\n", l_min);
    printf("%lx\n", ul_max);

    return 0;
}
