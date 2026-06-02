//# mode: run
//# exit: 0

extern int strcmp(const char *lhs, const char *rhs);

int main() {
    if (strcmp("alpha", "alpha") != 0)
        return 1;
    if (strcmp("alpha", "beta") >= 0)
        return 2;
    if (strcmp("beta", "alpha") <= 0)
        return 3;
    return 0;
}
