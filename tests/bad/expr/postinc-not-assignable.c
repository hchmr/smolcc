//# exit: 1
//# stderr: tests/bad/expr/postinc-not-assignable.c:4:5: error: operand not assignable
int main() {
    1 ++;
    return 0;
}
