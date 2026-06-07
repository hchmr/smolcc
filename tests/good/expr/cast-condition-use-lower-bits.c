//# description: Regression test ensuring that upper bits are either cleared by the cast or ignored by the condition code
//# mode: run

int main() {
    long value = 1L << 32;
    if ((int)value) {
        return 1;
    } else {
        return 0;
    }
}
