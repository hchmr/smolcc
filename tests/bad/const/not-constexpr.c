//# exit: 1
//# stderr: tests/bad/const/not-constexpr.c:6:13: error: expression cannot be evaluated at compile time
int f();

enum {
    Value = f(),
};
