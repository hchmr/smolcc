//# description: Verify that large integer literals are correctly loaded
//# mode: run

int main() {
    long value = 4294967296L;
    return value == (1L << 32) ? 0 : 1;
}
