//# stderr: tests/bad/const/not-constexpr.c:5:13: error: expression cannot be evaluated at compile time
int f();

enum {
    Value = f(),
};
