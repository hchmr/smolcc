//# mode: compile-only

static int value;  // tentative definition, internal linkage
extern int value;  // refers to previous, whose linkage is internal

int read_value() {
    return value;
}
