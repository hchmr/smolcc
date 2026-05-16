//# exit: 1
//# stderr: tests/bad/struct-param.c:7:27: error: bad parameter type
struct Pair {
	int left;
};

int take_pair(struct Pair pair);
