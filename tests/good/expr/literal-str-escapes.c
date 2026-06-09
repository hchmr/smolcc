//# mode: run
//# exit: 19

int main() {
    char *text = "a\n\t\?";
    return text[1] + text[2];
}
