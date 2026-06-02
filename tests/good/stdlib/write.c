//# mode: run
//# exit: 0
//# stdout: ok

extern long write(int fd, const void *buf, unsigned long count);

int main() {
    char text[3];
    text[0] = 'o';
    text[1] = 'k';
    text[2] = '\n';
    return write(1, text, 3) != 3;
}
