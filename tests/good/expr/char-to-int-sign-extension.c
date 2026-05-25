//# mode: run
//# exit: 255
//# desription: truncation to char happens first; the cast back to int must sign-extend.

int main() {
    return (int)(char)255;
}