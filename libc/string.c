int strlen(const char *s) {
    int n = 0;
    while (*s++)
        n++;
    return n;
}

int strcmp(const char *cs, const char *ct) {
    while (1) {
        if (*cs != *ct)
            return *cs - *ct;
        if (!*cs || !*ct)
            return 0;
        cs++, ct++;
    }
}

int strncmp(const char *cs, const char *ct, int n) {
    for (int i = 0; i < n; i++) {
        if (*cs != *ct)
            return *cs - *ct;
        if (!*cs || !*ct)
            return 0;
        cs++, ct++;
    }
    return 0;
}

void *memset(void *s, int c, int n) {
    char *p = s;
    for (int i = 0; i < n; i++) {
        p[i] = c;
    }
    return s;
}
