//# mode: run
//# stdout: -2147483648 -2147483648 -2147483648 -2147483648
//# stdout: 2147483647 2147483647 2147483647 2147483647
//# stdout: 255 255 255
//# stdout: 4096 4096 4096
//# stdout: 1024 1024 1024 1024
//# stdout: 0 255 255
//# stdout: 1024 256
//# stdout: 0 50 64

enum {
    INT_MIN_A = 1 << 31,
    INT_MIN_B = -2147483647 - 1,
    INT_MIN_C = (int)-2147483648L,
    INT_MIN_D = (int)(~0U << 31),

    INT_MAX_A = ~(1 << 31),
    INT_MAX_B = 2147483647,
    INT_MAX_C = (int)(~0U >> 1),
    INT_MAX_D = (unsigned int)-1 >> 1,

    BYTE_A = (1 << 8) - 1,
    BYTE_B = 240 | 15,
    BYTE_C = 170 ^ 85,

    PAGE_A = 1 << 12,
    PAGE_B = 64 * 64,
    PAGE_C = 1024 * 4,

    KIB_A = 1 << 10,
    KIB_B = 512 << 1,
    KIB_C = 2048 >> 1,
    KIB_D = 256 * 4,

    A = 170,
    B = 85,

    AND = A & B,
    OR = A | B,
    XOR = A ^ B,

    SHIFT_A = (1 << 5) << 5,
    SHIFT_B = 1024 >> 2,
    SHIFT_C = (64 * 2) >> 1,

    ADD_SUB_A = 100 + 23 - 123,
    ADD_SUB_B = 50 - 25 + 25,
};

extern int printf(const char *fmt, ...);

int main() {
    printf("%d %d %d %d\n", INT_MIN_A, INT_MIN_B, INT_MIN_C, INT_MIN_D);

    printf("%d %d %d %d\n", INT_MAX_A, INT_MAX_B, INT_MAX_C, INT_MAX_D);

    printf("%d %d %d\n", BYTE_A, BYTE_B, BYTE_C);

    printf("%d %d %d\n", PAGE_A, PAGE_B, PAGE_C);

    printf("%d %d %d %d\n", KIB_A, KIB_B, KIB_C, KIB_D);

    printf("%d %d %d\n", AND, OR, XOR);

    printf("%d %d\n", SHIFT_A, SHIFT_B);

    printf("%d %d %d\n", ADD_SUB_A, ADD_SUB_B, SHIFT_C);

    return 0;
}
