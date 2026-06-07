//# stderr: tests/bad/decl/struct-return-type.c:7:13: error: bad return type

struct Pair {
    int left;
};

struct Pair make_pair();
