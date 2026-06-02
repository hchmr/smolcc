//# mode: run
//# stdout: 4294967295
//# stdout: 18446744073709551615

extern int printf(const char *fmt, ...);

unsigned int u = 4294967295U;
unsigned long ul = 18446744073709551615UL;

int main() {
    printf("%u\n", u);
    printf("%lu\n", ul);
    return 0;
}
