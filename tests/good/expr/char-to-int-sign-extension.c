//# description: truncation to char happens first; the cast back to int must sign-extend.
//# mode: run
//# exit: 255

int main() {
    return (int)(char)255;
}
