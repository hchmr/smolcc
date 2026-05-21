//# exit: 1
//# stderr: tests/bad/struct-return.c:8:13: error: bad function return type

struct Pair {
    int left;
};

struct Pair make_pair();
