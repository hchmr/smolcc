//# mode: run

int main() {
    return (sizeof(char) == 1 &&
            sizeof(unsigned char) == 1 &&
            sizeof(int) == 4 &&
            sizeof(unsigned int) == 4 &&
            sizeof(long) == 8 &&
            sizeof(unsigned long) == 8) ? 0 : 1;
}
