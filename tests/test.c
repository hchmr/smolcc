extern int ext_counter;
extern int ext_counter;
int ext_counter;

extern int declared_only(int value);

enum {
    Mode_None,
    Mode_Read = 4,
    Mode_Write,
    Mode_All = Mode_Read + Mode_Write
};

int alpha, gamma;
static int internal_counter;
static long internal_total;
static int merged_linkage_obj;
extern int merged_linkage_obj;
int mode_default = Mode_All;
long counts[Mode_All][2];

struct Forward;
struct Forward *forward_ptr;

struct Pair {
    int left;
    long right;
};

struct Node {
    int value;
    struct Node *next;
};

struct Matrix {
    int rows[2];
    long cols[2][2];
};

struct Forward {
    int ready;
};

int matrix_size = sizeof(struct Matrix);

extern struct Pair external_pair;
struct Pair external_pair;
struct Pair *pair_ptr;
struct Node head;
struct Forward forward_value;
struct {
    int bits;
} anon_global;

int zero();
int pair_left(struct Pair *pair);
int use_arrays(int values[2], char text[3]);
int use_pointers(void *opaque, char *text, struct Pair *pair);

int zero() {
    return 0;
}

static long widen(int value) {
    long result;
    result = value;
    return result;
}

int pair_left(struct Pair *pair) {
    return pair->left;
}

int use_arrays(int values[2], char text[3]) {
    return values[0] + text[0];
}

void use_func_pointers(int func(int)) {
    // calling function pointers is not supported yet
}

int use_pointers(void *opaque, char *text, struct Pair *pair) {
    return opaque ? pair->left + text[0] : pair->right;
}

int condition_examples() {
    if ("x")
        merged_linkage_obj = merged_linkage_obj + 1;
    if (zero)
        merged_linkage_obj = merged_linkage_obj + 1;
    anon_global.bits = merged_linkage_obj;
    return anon_global.bits;
}

int declarations() {
    enum {
        Local_A,
        Local_B = 7,
        Local_C
    };
    int a, b;
    long total;
    char ch;
    int values[2];
    char text[4];
    struct Pair pair;
    struct Node node;
    struct Forward *forward_local;

    a = Local_A + 1;
    b = Local_C;
    total = widen(a + b);
    ch = '\\';

    values[0] = a;
    values[1] = b;
    text[0] = 'x';
    text[1] = '\n';
    text[2] = 0;
    text[3] = *"hello";
    char *escaped = "a\n\t\?";

    pair.left = values[0];
    pair.right = total;
    pair.right = pair.right + pair_left(&pair);
    node.value = pair.left + condition_examples();
    node.next = &head;
    forward_local = &forward_value;

    if (forward_local == 0) {
        int fallback;
        fallback = zero();
        total = total + fallback;
    } else {
        total = total + use_arrays(values, text);
    }

    while (a < 3) {
        int loop_local;
        loop_local = a;
        a = a + 1;
        total = total + loop_local;
        if (loop_local == 1)
            continue;
        if (loop_local == 2)
            break;
    }

    {
        int shadow;
        shadow = use_pointers(forward_local, escaped, &pair);
        internal_counter = shadow;
        internal_total = total;
    }

    gamma = ch + escaped[1];
    return internal_counter + internal_total + node.value + matrix_size;
}
