//# mode: run
//# stdout: 4294967295
//# stdout: 2147483648
//# stdout: 9223372036854775807
//# stdout: 9223372036854775808
//# exit: 0

extern int printf(const char *fmt, ...);

int main() {
    printf("%u\n", 4294967295U);
    printf("%ld\n", 2147483648L);
    printf("%ld\n", 9223372036854775807L);
    printf("%lu\n", 9223372036854775808UL);
    return 0;
}
