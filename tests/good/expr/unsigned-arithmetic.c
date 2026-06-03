//# mode: run
//# exit: 0
//# stdout: 4294967295
//# stdout: 0
//# stdout: 2147483647
//# stdout: 1
//# stdout: 1
//# stdout: 4294967295
//# stdout: 1
//# stdout: 1
//# stdout: 1
//# stdout: 1
//# stdout: 1
//# stdout: 4294967295
//# stdout: 1
//# stdout: 0
//# stdout: 1
//# stdout: 4294967294

extern int printf(const char *fmt, ...);

int main() {
    unsigned int max_u = 0u - 1u;
    unsigned int zero = max_u + 1u;
    unsigned int half = max_u >> 1;
    printf("%u\n", max_u);
    printf("%u\n", zero);
    printf("%u\n", half);

    printf("%d\n", max_u > half);
    printf("%d\n", zero < max_u);

    unsigned int explicit_neg = (unsigned int)-1;
    printf("%u\n", explicit_neg);

    printf("%d\n", (max_u >> 31) == 1u);
    printf("%d\n", (1u << 31) > half);

    printf("%d\n", (zero - 1u) > zero);
    printf("%d\n", (max_u + 1u) < max_u);
    printf("%d\n", (max_u + 2u) == 1u);

    printf("%u\n", ~zero);
    printf("%d\n", ~max_u == zero);

    printf("%u\n", (half + 1u) * 2u);
    printf("%u\n", max_u * max_u);
    printf("%u\n", max_u * 2u);

    return 0;
}
