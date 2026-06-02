//# mode: run
//# stdout: 200
//# stdout: 44
//# stdout: -56
//# stdout: 1

extern int printf(const char *fmt, ...);

int main() {
    unsigned char uc = 200;
    printf("%d\n", uc + 0);

    unsigned char a = 200;
    unsigned char b = 100;
    unsigned char sum = a + b;
    printf("%d\n", sum + 0);

    char sc = (char)200;
    printf("%d\n", sc + 0);

    printf("%d\n", (int)sizeof(unsigned char));
    return 0;
}
