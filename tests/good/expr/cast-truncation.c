//# description: Verify that casts to smaller integer types properly truncate the value
//# mode: run
//# stdout: 0x76543210
//# stdout: 0x21
//# stdout: 0x76543210
//# stdout: 0x21

extern int printf(const char *fmt, ...);

int main() {
    int long_to_int = (int)1985229328L;  // 0x76543210
    int int_to_char = (char)124076833;  // 0x7654321
    int ulong_to_uint = (unsigned int)1985229328UL;  // 0xfedcba9876543210
    unsigned int uint_to_uchar = (unsigned char)2271560481U;  // 0x87654321

    printf("%#x\n", long_to_int);
    printf("%#x\n", int_to_char);
    printf("%#x\n", ulong_to_uint);
    printf("%#x\n", uint_to_uchar);

    return 0;
}
