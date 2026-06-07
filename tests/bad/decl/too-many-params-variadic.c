//# description: variadic declarations still count against the same fixed parameter limit.
//# stderr: tests/bad/decl/too-many-params-variadic.c:4:5: error: too many parameters

int sum(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, ...);

int main() {
    return sum(0, 1, 2, 3, 4, 5, 6, 7, 8);
}
