//# mode: run
//# stdout: 65
//# stdout: 127
//# stdout: -128
//# stdout: 123
//# stdout: 255
//# stdout: 0
//# stdout: 1116412323
//# stdout: 2147483647
//# stdout: -2147483648
//# stdout: 3216412323
//# stdout: 4294967295
//# stdout: 461141232384756123
//# stdout: 9223372036854775807
//# stdout: -9223372036854775808
//# stdout: 155042132384756123
//# stdout: 18446744073709551615

extern int printf(const char *fmt, ...);

char c = 'A';
char c_max = 127;
char c_min = -128;

unsigned char uc = 123;
unsigned char uc_max = 255;
unsigned char uc_min = 0;

int i = 1116412323;
int i_max = 2147483647;
int i_min = -2147483647 - 1;

unsigned int u = 3216412323;
unsigned int u_max = 4294967295;

long l = 461141232384756123;
long l_max = 9223372036854775807;
long l_min = -9223372036854775807 - 1;

unsigned long ul = 155042132384756123;
unsigned long ul_max = 18446744073709551615U;

int main() {
    printf("%d\n", c);
    printf("%d\n", c_max);
    printf("%d\n", c_min);
    printf("%u\n", uc);
    printf("%u\n", uc_max);
    printf("%u\n", uc_min);
    printf("%d\n", i);
    printf("%d\n", i_max);
    printf("%d\n", i_min);
    printf("%u\n", u);
    printf("%u\n", u_max);
    printf("%ld\n", l);
    printf("%ld\n", l_max);
    printf("%ld\n", l_min);
    printf("%lu\n", ul);
    printf("%lu\n", ul_max);
    return 0;
}
