//# mode: run

int main() {
    int count = 0;
    int i = 0;
    while (i < 3) {
        int j = 0;
        while (j < 3) {
            if (j == 1)
                break;
            count = count + 1;
            j = j + 1;
        }
        i = i + 1;
    }
    return count == 3 ? 0 : 1;
}
