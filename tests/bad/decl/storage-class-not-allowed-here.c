//# stderr: tests/bad/decl/storage-class-not-allowed-here.c:3:5: error: storage class specifier is not allowed here
struct Box {
    static int value;
};
