//# stderr: tests/bad/const/not-constant.c:5:12: error: expression cannot be evaluated at compile time
int value;

enum {
    Size = value,
};
