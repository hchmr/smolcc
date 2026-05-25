//# exit: 1
//# stderr: tests/bad/decl/function-decl-not-allowed-here.c:4:9: error: function declaration is not allowed here
struct Box {
    int value();
};
