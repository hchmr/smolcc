//# exit: 1
//# stderr: tests/bad/const/not-constant.c:6:12: error: expression cannot be evaluated at compile time
int value;

enum {
    Size = value,
};
