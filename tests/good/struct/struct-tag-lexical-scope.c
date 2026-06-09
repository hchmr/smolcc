//# mode: compile-only

void nested1() {
    struct node {
        int value;
    };
}

struct node {
    int value;
    struct node *next;
} global;

void nested2() {
    struct node {
        const char *name;
    };
}

int main() {
    return 0;
}
