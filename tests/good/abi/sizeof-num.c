//# mode: run
//# stdout: 1,1,4,4,8,8

extern int printf(const char *fmt, ...);

int main() {
    printf("%lu,", sizeof(char));
    printf("%lu,", sizeof(unsigned char));
    printf("%lu,", sizeof(int));
    printf("%lu,", sizeof(unsigned int));
    printf("%lu,", sizeof(long));
    printf("%lu", sizeof(unsigned long));
    return 0;
}
