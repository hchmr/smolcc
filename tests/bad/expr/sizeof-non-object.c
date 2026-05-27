//# exit: 1
//# stderr: tests/bad/expr/sizeof-non-object.c:4:12: error: sizeof operand must be object
int main() {
    return sizeof(void);
}
