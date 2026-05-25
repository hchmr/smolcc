//# mode: compile-only

// From N1256: 6.9.2 External definitions

int i1 = 1;  // definition, external linkage
static int i2 = 2;  // definition, internal linkage
extern int i3 = 3;  // definition, external linkage
int i4;  // tentative definition, external linkage
static int i5;  // tentative definition, internal linkage

int i1;  // valid tentative definition, refers to previous
int i3;  // valid tentative definition, refers to previous
int i4;  // valid tentative definition, refers to previous

extern int i1;  // refers to previous, whose linkage is external
extern int i2;  // refers to previous, whose linkage is internal
extern int i3;  // refers to previous, whose linkage is external
extern int i4;  // refers to previous, whose linkage is external
extern int i5;  // refers to previous, whose linkage is internal
