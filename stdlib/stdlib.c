extern int isspace(int c);
extern int isxdigit(int c);

enum {
    ERANGE = 34,
};
extern int errno;

int digit_to_int_in(int base, int c) {
    int digit = base;
    if (c >= '0' && c <= '9')
        digit = c - '0';
    else if (c >= 'a' && c <= 'z')
        digit = c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z')
        digit = c - 'A' + 10;
    return digit >= base ? -1 : digit;
}

static int try_parse(const char *s, int base, int *negp, unsigned long *valp, const char **endp) {
    const char *initial = s;

    if (base < 0 || base == 1 || base > 36) {
        base = 36;
    }

    while (isspace(*s)) {
        s++;
    }

    int neg = 0;
    if (*s == '+' || *s == '-') {
        neg = *s++ == '-';
    }

    const char *before_digits = s;

    if ((base == 0 || base == 16) && *s == '0' && (s[1] == 'x' || s[1] == 'X') && isxdigit(s[2])) {
        s = s + 2;
        base = 16;
    } else if (base == 0 && *s == '0') {
        base = 8;
    } else if (base == 0) {
        base = 10;
    }

    unsigned long n = 0;
    int overflow = 0;

    for (int digit; (digit = digit_to_int_in(base, *s)) >= 0; s++) {
        if (n > (~0UL - digit) / base) {
            overflow = 1;
            break;
        }
        n = n * base + digit;
    }

    while (digit_to_int_in(base, *s) >= 0) {
        s++;
    }

    *negp = neg;
    if (endp) {
        *endp = s == before_digits ? initial : s;
    }

    *valp = n;
    return !overflow;
}

unsigned long strtoul(const char *s, const char **endp, int base) {
    unsigned long ulong_max = ~0UL;

    int neg;
    unsigned long n;
    if (!try_parse(s, base, &neg, &n, endp)) {
        errno = ERANGE;
        return ulong_max;
    }
    return neg ? -n : n;
}

long strtol(const char *s, const char **endp, int base) {
    long long_min = (long)(1UL << (sizeof(long) * 8 - 1));
    long long_max = (long)(~0UL >> 1);

    unsigned long n;
    int neg;
    if (!try_parse(s, base, &neg, &n, endp)) {
        errno = ERANGE;
        return neg ? long_min : long_max;
    }

    if (neg) {
        if (n > (unsigned long)-(long_min + 1) + 1) {
            errno = ERANGE;
            return long_min;
        }
        return -n;
    } else {
        if (n > (unsigned long)long_max) {
            errno = ERANGE;
            return long_max;
        }
        return n;
    }
}

long atol(const char *s) {
    return strtol(s, 0, 10);
}

int atoi(const char *s) {
    return (int)atol(s);
}
