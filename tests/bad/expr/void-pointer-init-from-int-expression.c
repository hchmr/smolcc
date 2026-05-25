//# exit: 1
//# stderr: tests/bad/expr/void-pointer-init-from-int-expression.c:5:11: error: target type mismatch
//# desription: this folds to zero, but it still has integer type rather than being a null pointer constant in this subset.

void *p = 0 - 0;

int main() {
    return 0;
}