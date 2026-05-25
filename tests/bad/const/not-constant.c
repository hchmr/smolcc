//# exit: 1
//# stderr: tests/bad/const/not-constant.c:6:12: error: not a constant
int value;

enum {
    Size = value,
};
