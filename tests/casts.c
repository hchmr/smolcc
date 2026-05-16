//# mode: run
//# exit: 0
char global_zero = 256;
void *global_null = 0;

int main() {
    if ((char)256)
        return 1;
    if (global_zero)
        return 2;
    if ((int)(char)255 != -1)
        return 3;
    if (global_null != 0)
        return 4;
    return 0;
}
