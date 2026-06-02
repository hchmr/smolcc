//# mode: run
//# stdout: -2147483648
//# stdout: 2147483647
//# stdout: -128
//# stdout: 127
//# stdout: -1
//# stdout: -701990824
//# stdout: 0
//# stdout: -1
//# stdout: 127
//# stdout: -128
//# stdout: 255

enum {
    INT_MIN = (int)-2147483648L,
    INT_MAX = 2147483647,
    CHAR_MIN = (char)-128,
    CHAR_MAX = (char)127,

    UINT_MAX_TO_INT = (int)(4294967295U),
    UINT_TO_INT = (int)3592976472U,
    LONG_TO_INT = (int)1099511627776L,
    ULONG_TO_INT = (int)1099511627775UL,
    CHAR_MAX_TO_INT = (int)(char)CHAR_MAX,
    CHAR_MIN_TO_INT = (int)(char)CHAR_MIN,
    UCHAR_MAX_TO_INT = (int)(unsigned char)255,
};

extern int printf(const char *fmt, ...);

int main() {
    printf("%d\n", INT_MIN);
    printf("%d\n", INT_MAX);
    printf("%d\n", CHAR_MIN);
    printf("%d\n", CHAR_MAX);
    printf("", UINT_MAX_TO_INT);
    printf("%d\n", UINT_MAX_TO_INT);
    printf("%d\n", UINT_TO_INT);
    printf("%d\n", LONG_TO_INT);
    printf("%d\n", ULONG_TO_INT);
    printf("%d\n", CHAR_MAX_TO_INT);
    printf("%d\n", CHAR_MIN_TO_INT);
    printf("%d\n", UCHAR_MAX_TO_INT);
    return 0;
}
