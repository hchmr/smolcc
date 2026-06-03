//# mode: run
//# exit: 0
//# description: Verify that large integer literals are correctly loaded

int main() {
    long value = 4294967296L;
    return value == (1L << 32) ? 0 : 1;
}
