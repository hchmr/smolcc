//# mode: run
//# exit: 7

struct Node;
struct Node *link;

struct Node {
    int value;
};

int main() {
    struct Node node;
    link = &node;
    link->value = 7;
    return link->value;
}