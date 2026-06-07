//# stderr: tests/bad/const/bad-cast.c:3:13: error: bad cast in constant expression
enum {
    Value = (void *)1,
};
