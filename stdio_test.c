extern int printf(const char *format, ...);

int main() {
    //= BASIC LITERALS & MULTI-ARGUMENT HANDLING

    printf("--- Basics & argument order ---\n");
    printf("Literal string with no args\n");
    printf("Escape chars: \\n=\n, \\t=\t, percent=%%\n");
    printf("Two integers: x=%i y=%i\n", 420, 71234);
    printf("Mixed types: %s %i %c\n", "hello", 123, 'A');
    printf("Null string: |%s|\n", (char *)0);

    //= SIGNED DECIMAL EDGE CASES (%d, %i)

    printf("\n--- Decimal edge cases ---\n");
    printf("Zero:         |%d|\n", 0);
    printf("One:          |%d|\n", 1);
    printf("Minus one:    |%d|\n", -1);
    printf("Max int (32): |%d|\n", 2147483647);
    printf("Min int (32): |%d|\n", -2147483647 - 1);
    printf("%%i specifier: |%i|\n", 123);  // should be same as %d

    //= HEXADECIMAL & OCTAL EDGE CASES (%x, %X, %o)

    printf("\n--- Hex & octal ---\n");
    printf("Hex zero:     |%x|\n", 0);
    printf("Hex upper:    |%x|\n", 11259375);
    printf("Hex lower:    |%X|\n", 11259375);
    printf("Hex positive: |%x|\n", 123);
    printf("Hex minus one:|%x|\n", -1);
    printf("Hex min int:  |%x|\n", -2147483647 - 1);

    printf("Oct zero:     |%o|\n", 0);
    printf("Oct positive: |%o|\n", 123);
    printf("Oct minus one:|%o|\n", -1);
    printf("Oct min int:  |%o|\n", -2147483647 - 1);

    //= SIGN MODIFIER FLAGS ('+' and ' ')

    printf("\n--- Sign Flags ('+' and ' ') ---\n");
    printf("' ' pos:  |% d|\n", 123);
    printf("' ' neg:  |% d|\n", -123);
    printf("'+' pos:  |%+d|\n", 123);
    printf("'+' neg:  |%+d|\n", -123);
    printf("'+' zero: |%+d|\n", 0);
    printf("' ' zero: |% d|\n", 0);

    //= MINIMUM WIDTH & ZERO PADDING FIELD CONFLICTS

    printf("\n--- Width, padding & alignment ---\n");
    printf("Right padding:      |%10d|\n", 123);
    printf("Left padding (-):   |%-10d|\n", 123);
    printf("Zero padding (0):   |%010d|\n", 123);
    printf("Plus + width:       |%+10d|\n", 123);
    printf("Plus + zero pad:    |%0+10d|\n", 123);
    printf("Left adj + '0':     |%-010d|\n", 123);
    printf("Left adj + '+':     |%-+10d|\n", 123);
    printf("Left adj + ' ':     |%- 10d|\n", 123);
    printf("Left adj + '0 ':    |%-0 10d|\n", 123);

    //= ALTERNATE FORM FLAGS (#)

    printf("\n--- Alternate form flags (#) ---\n");
    printf("Alt hex zero: |%#x|\n", 0);
    printf("Alt hex pos:  |%#x|\n", 255);
    printf("Alt hex upper:|%#X|\n", 255);
    printf("Alt hex neg:  |%#x|\n", -1);
    printf("Alt oct zero: |%#o|\n", 0);
    printf("Alt oct pos:  |%#o|\n", 8);

    //= STRINGS & CHARACTERS WITH PADDING (%s, %c)

    printf("\n--- Strings and characters ---\n");
    printf("Normal str:   |%s|\n", "test");
    printf("Width str:    |%10s|\n", "test");
    printf("Left adj str: |%-10s|\n", "test");
    printf("Char normal:  |%c|\n", 'X');
    printf("Char width:   |%5c|\n", 'X');

    //= %p SPECIFIER

    printf("\n--- pointer printing ---\n");
    // void *p1 = (void *)0xfedcba9876543210;
    int lower32 = 1985229328;
    int upper32 = 2147483647;
    struct { int low; int high; } bits;
    bits.low = lower32;
    bits.high = upper32;
    printf("High ptr: |%p|\n", *(void**)&bits);
    printf("Null ptr: |%p|\n", (void *)0);

    //= %% SPECIFIER

    printf("\n--- Literal percent sign (%%) ---\n");
    printf("Percent: |%%|\n");
    printf("Multiple: |%%%%%%|\n");
    printf("Confusing: |%%%%s%%%s%%|\n", "s");
    printf("Width + percent: |%10%|\n");

    //= %n SPECIFIER

    int n1 = 0, n2 = 0;
    printf("\n--- %%n specifier ---\n");
    printf("Hello%n World%n!\n", &n1, &n2);
    printf("5=%i 11=%i\n", n1, n2);
    return 0;
}
