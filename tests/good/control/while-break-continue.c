//# mode: run
//# exit: 8

int main() {
    int i = 0;
    int sum = 0;
    while (i < 5) {
        i = i + 1;
        if (i == 2)
            continue;
        sum = sum + i;
        if (i == 4)
            break;
    }
    return sum;
}