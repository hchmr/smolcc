//# exit: 1
//# stderr: tests/bad/const/bad-cast.c:4:13: error: bad cast in constant expression
enum {
    Value = (void *)1,
};
