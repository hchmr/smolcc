//# mode: run
//# stdout: 10000000000
//# stdout: 4294967296
//# stdout: -1
//# stdout: 9223372036854775807

extern int printf(const char *fmt, ...);

int main() {
    printf("%ld\n", 100000L * 100000L);
    printf("%ld\n", 1L << 32);
    printf("%ld\n", -2L >> 1);
    printf("%ld\n", 9223372036854775807L + 0L);
    return 0;
}
