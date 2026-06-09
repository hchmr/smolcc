//# mode: run
//# exit: 50

int main() {
    unsigned int sum = 0U;

    // Downward loop passing zero into underflow (UINT_MAX)
    for (unsigned int i = 5U; i <= 5U; i--) {
        sum = sum + i;
    }

    // Upward loop passing UINT_MAX into overflow (0)
    for (unsigned int j = 0u - 3u; j >= 0u - 3u; j++) {
        sum = sum + 1U;
    }

    // Strided multiplication loop crossing boundary
    for (unsigned int k = 1U; k != 0U; k = k * 2U) {
        sum = sum + 1U;
    }

    return sum;
}
