//# mode: run
//# stdout: --- Basics & argument order ---
//# stdout: Literal string with no args
//# stdout: Escape chars: \n=
//# stdout: ", \\t=\t, percent=%"
//# stdout: Two integers: x=420 y=71234
//# stdout: Mixed types: hello 123 A
//# stdout: Null string: |(null)|
//# stdout: ""
//# stdout: --- Decimal edge cases ---
//# stdout: Zero:         |0|
//# stdout: One:          |1|
//# stdout: Minus one:    |-1|
//# stdout: Max int (32): |2147483647|
//# stdout: Min int (32): |-2147483648|
//# stdout: %i specifier: |123|
//# stdout: ""
//# stdout: --- Unsigned decimal ---
//# stdout: Unsigned int max:  |4294967295|
//# stdout: Unsigned long max: |18446744073709551615|
//# stdout: ""
//# stdout: --- Long decimal ---
//# stdout: Long decimal min: |-9223372036854775808|
//# stdout: Long decimal max: |9223372036854775807|
//# stdout: ""
//# stdout: --- Hex & octal ---
//# stdout: Hex zero:     |0|
//# stdout: Hex upper:    |abcdef|
//# stdout: Hex lower:    |ABCDEF|
//# stdout: Hex positive: |7b|
//# stdout: Hex minus one:|ffffffff|
//# stdout: Hex min int:  |80000000|
//# stdout: Oct zero:     |0|
//# stdout: Oct positive: |173|
//# stdout: Oct minus one:|37777777777|
//# stdout: Oct min int:  |20000000000|
//# stdout: Long min hex: |8000000000000000|
//# stdout: Long max hex: |7fffffffffffffff|
//# stdout: Long min oct: |1000000000000000000000|
//# stdout: Long max oct: |777777777777777777777|
//# stdout: ""
//# stdout: --- Sign Flags ('+' and ' ') ---
//# stdout: ' ' pos:  | 123|
//# stdout: ' ' neg:  |-123|
//# stdout: '+' pos:  |+123|
//# stdout: '+' neg:  |-123|
//# stdout: '+' zero: |+0|
//# stdout: ' ' zero: | 0|
//# stdout: ""
//# stdout: --- Width, padding & alignment ---
//# stdout: Right padding:      |       123|
//# stdout: Left padding (-):   |123       |
//# stdout: Zero padding (0):   |0000000123|
//# stdout: Plus + width:       |      +123|
//# stdout: Plus + zero pad:    |+000000123|
//# stdout: Left adj + '0':     |123       |
//# stdout: Left adj + '+':     |+123      |
//# stdout: Left adj + ' ':     | 123      |
//# stdout: Left adj + '0 ':    | 123      |
//# stdout: ""
//# stdout: --- Alternate form flags (#) ---
//# stdout: Alt hex zero: |0|
//# stdout: Alt hex pos:  |0xff|
//# stdout: Alt hex upper:|0XFF|
//# stdout: Alt hex neg:  |0xffffffff|
//# stdout: Alt oct zero: |0|
//# stdout: Alt oct pos:  |010|
//# stdout: ""
//# stdout: --- Strings and characters ---
//# stdout: Normal str:   |test|
//# stdout: Width str:    |      test|
//# stdout: Left adj str: |test      |
//# stdout: Char normal:  |X|
//# stdout: Char width:   |    X|
//# stdout: ""
//# stdout: --- pointer printing ---
//# stdout: High ptr: |0xc5f467a193b28ed|
//# stdout: Low ptr:  |0xfe63|
//# stdout: Null ptr: |(nil)|
//# stdout: ""
//# stdout: --- Literal percent sign (%) ---
//# stdout: Percent: |%|
//# stdout: Multiple: |%%%|
//# stdout: Confusing: |%%s%s%|
//# stdout: Width + percent: |%|
//# stdout: ""
//# stdout: --- %n specifier ---
//# stdout: Hello World!
//# stdout: 5=5 11=11

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

    printf("\n--- Unsigned decimal ---\n");
    printf("Unsigned int max:  |%u|\n", 4294967295U);
    printf("Unsigned long max: |%lu|\n", 18446744073709551615UL);

    //= LONG DECIMAL

    printf("\n--- Long decimal ---\n");
    printf("Long decimal min: |%ld|\n", (long)1 << 63);
    printf("Long decimal max: |%ld|\n", ~((long)1 << 63));

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

    printf("Long min hex: |%lx|\n", (long)1 << 63);
    printf("Long max hex: |%lx|\n", ~((long)1 << 63));
    printf("Long min oct: |%lo|\n", (long)1 << 63);
    printf("Long max oct: |%lo|\n", ~((long)1 << 63));

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
    // void *p1 = (void *)0xc5f467a193b28ed;
    void *p;
    ((int *)&p)[0] = 423307501; // lower
    ((int *)&p)[1] = 207570554; // upper
    printf("High ptr: |%p|\n", p);
    printf("Low ptr:  |%p|\n", (void*) 65123);
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
