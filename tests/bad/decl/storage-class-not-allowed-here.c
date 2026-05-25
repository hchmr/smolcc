//# exit: 1
//# stderr: tests/bad/decl/storage-class-not-allowed-here.c:4:5: error: storage class specifier is not allowed here
struct Box {
    static int value;
};
