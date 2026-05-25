//# mode: run
//# exit: 9

enum {
    Mode_None,
    Mode_Read = 4,
    Mode_Write,
};

int main() {
    return Mode_None + Mode_Read + Mode_Write;
}
