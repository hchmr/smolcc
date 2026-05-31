//# description: Verifies strcmp/strncmp compare bytes as unsigned char values.
//# mode: run
//# exit: 0

extern int strcmp(const char *lhs, const char *rhs);
extern int strncmp(const char *lhs, const char *rhs, int n);

int main() {
    char s[2];
    s[0] = -1;
    s[1] = 0;

    if (strcmp(s, "") <= 0)
        return 1;
    if (strncmp(s, "", 1) <= 0)
        return 2;

    return 0;
}
