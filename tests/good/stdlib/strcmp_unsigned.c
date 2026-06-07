//# description: Verifies strcmp/strncmp compare bytes as unsigned char values.
//# mode: run

extern unsigned long strcmp(const char *lhs, const char *rhs);
extern unsigned long strncmp(const char *lhs, const char *rhs, unsigned long n);

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
