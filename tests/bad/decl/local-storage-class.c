//# stderr: tests/bad/decl/local-storage-class.c:3:5: error: storage class specifier is not allowed here
int main() {
    static int value;
    return 0;
}
