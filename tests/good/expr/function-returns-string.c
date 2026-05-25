//# mode: run
//# exit: 120

char *ret_text() {
    return "x";
}

int main() {
    return ret_text()[0];
}