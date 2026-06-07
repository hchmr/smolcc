//# description: this folds to zero, but it still has integer type rather than being a null pointer constant in this subset.
//# stderr: tests/bad/expr/void-pointer-init-from-int-expression.c:4:11: error: target type mismatch

void *p = 0 - 0;

int main() {
    return 0;
}
