//# exit: 1
//# stderr: tests/bad/lex/expected-expression.c:4:9: error: expected expression, got ')'
int main() {
    if ()
        return 1;
    return 0;
}
