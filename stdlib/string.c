int strlen(const char *s) {
    int n = 0;
    while (*s++)
        n++;
    return n;
}

int strcmp(const char *s1, const char *s2) {
    while (1) {
        int c1 = *s1 & 255;
        int c2 = *s2 & 255;
        if (c1 != c2)
            return c1 - c2;
        if (c1 == 0)
            return 0;
        s1++, s2++;
    }
}

int strncmp(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        int c1 = *s1 & 255;
        int c2 = *s2 & 255;
        if (c1 != c2)
            return c1 - c2;
        if (c1 == 0)
            return 0;
        s1++, s2++;
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
