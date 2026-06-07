//# stderr: tests/bad/expr/sizeof-non-object.c:3:12: error: sizeof operand must be object
int main() {
    return sizeof(void);
}
