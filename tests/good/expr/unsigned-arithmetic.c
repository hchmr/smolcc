//# mode: run
//# stdout: 4294967295
// smolcc doesn't handle unsigned overflow correctly. The second line of output is 4294967296 instead of 0
// //# stdout: 0
//# stdout: 2147483647
//# stdout: 1
//# stdout: 1

extern int printf(const char *fmt, ...);

int main() {
    unsigned int a = 0U - 1U;
    unsigned int b = 4294967295U;
    printf("%u\n", a);
    // printf("%u\n", b + 1U);
    printf("%u\n", b >> 1);
    printf("%d\n", a > 2147483647);
    printf("%d\n", a == b);
    return 0;
}
