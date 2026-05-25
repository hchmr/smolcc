//# exit: 1
//# stderr: tests/bad/decl/struct-parameter.c:8:27: error: bad parameter type

struct Pair {
    int left;
};

int take_pair(struct Pair pair);