// codex.cpp - the C Codex's entries and what unlocks them (see codex.h).

#include "codex.h"
#include <cctype>

const std::vector<CodexEntry> &codex_entries()
{
    static const std::vector<CodexEntry> list = {
        // ----- C basics -----
        { "printf", "printf", "stdio.h", "basics", "int printf(const char *format, ...);",
          "Prints text. Each % in the format is filled in by the next value: %d an int, %f a double, %c a char, %s a string.",
          "#include <stdio.h>\n\nint main(void) {\n    int age = 19;\n    double gpa = 3.75;\n    printf(\"%d years, %.1f GPA\\n\", age, gpa);\n    printf(\"%c%c\\n\", 'O', 'K');\n    return 0;\n}\n",
          "19 years, 3.8 GPA\nOK", { "%d with a double prints rubbish: the type has to match", "forgetting \\n, so the next print joins the line" } },
        { "scanf", "scanf", "stdio.h", "basics", "int scanf(const char *format, ...);",
          "Reads typed input into variables. It needs their addresses (&x), and returns how many it read.",
          "#include <stdio.h>\n\nint main(void) {\n    int n = 0;\n    // MAL-OS gives no keyboard input, so scanf reads nothing:\n    int got = scanf(\"%d\", &n);\n    printf(\"read %d, n is still %d\\n\", got < 0 ? 0 : got, n);\n    return 0;\n}\n",
          "read 0, n is still 0", { "scanf(\"%d\", n) without the &: it writes to a random address", "not checking what it returns" } },
        { "sizeof", "sizeof", "operator", "basics", "sizeof(type)   sizeof variable",
          "How many bytes a type or variable takes. For an array it's the whole array.",
          "#include <stdio.h>\n\nint main(void) {\n    int a[5];\n    printf(\"%d %d\\n\", (int)sizeof(int), (int)sizeof(char));\n    printf(\"%d\\n\", (int)(sizeof(a) / sizeof(a[0])));\n    return 0;\n}\n",
          "4 1\n5", { "sizeof on an array passed to a function gives the pointer's size", "malloc(n) instead of malloc(n * sizeof(int))" } },
        { "for", "for", "keyword", "control", "for (start; keep going while; step) { ... }",
          "Repeats its body: start runs once, the condition is checked before each go, the step runs after each go.",
          "#include <stdio.h>\n\nint main(void) {\n    for (int i = 0; i < 5; i++)\n        printf(\"%d \", i * i);\n    printf(\"\\n\");\n    return 0;\n}\n",
          "0 1 4 9 16", { "i <= n instead of i < n: one too many (the Off-By-One)", "a ; straight after the ) gives it an empty body" } },
        { "while", "while / do", "keyword", "control", "while (cond) { ... }    do { ... } while (cond);",
          "while checks first, so it may never run. do-while runs once, then checks.",
          "#include <stdio.h>\n\nint main(void) {\n    int n = 1;\n    while (n < 50)\n        n *= 2;\n    printf(\"%d\\n\", n);\n    do { n--; } while (0);\n    printf(\"%d\\n\", n);\n    return 0;\n}\n",
          "64\n63", { "never changing the variable in the condition: an infinite loop", "forgetting the ; after do-while's )" } },
        { "switch", "switch", "keyword", "control", "switch (value) { case 1: ... break; default: ... }",
          "Jumps to the case that matches. Without break it falls through into the next case too.",
          "#include <stdio.h>\n\nint main(void) {\n    int day = 6;\n    switch (day) {\n        case 6:\n        case 7: printf(\"weekend\\n\"); break;\n        default: printf(\"weekday\\n\");\n    }\n    return 0;\n}\n",
          "weekend", { "a missing break runs the next case as well", "case only takes constants: no ranges or strings" } },
        { "array", "arrays [ ]", "type", "arrays", "type name[size];",
          "A row of values side by side in memory. The first is [0] and the last is [size - 1].",
          "#include <stdio.h>\n\nint main(void) {\n    int marks[4] = {70, 85, 62};\n    int total = 0;\n    for (int i = 0; i < 4; i++)\n        total += marks[i];\n    printf(\"%d %d\\n\", marks[3], total);\n    return 0;\n}\n",
          "0 217", { "marks[4] in a 4-long array: one past the end", "copying arrays with = (you can't: copy each element)" } },
        { "array2d", "2D arrays", "type", "arrays", "type name[rows][cols];",
          "A grid: an array of rows. g[r][c] is row r, column c. In memory, row 0 comes first, then row 1.",
          "#include <stdio.h>\n\nint main(void) {\n    int g[2][3] = {{1, 2, 3}, {4, 5, 6}};\n    for (int r = 0; r < 2; r++) {\n        for (int c = 0; c < 3; c++)\n            printf(\"%d\", g[r][c]);\n        printf(\"\\n\");\n    }\n    return 0;\n}\n",
          "123\n456", { "mixing up rows and columns", "passing it to a function without the column size: int g[][3]" } },
        { "strlen", "strlen", "string.h", "strings", "size_t strlen(const char *s);",
          "Counts the characters before the '\\0'. The '\\0' itself isn't counted.",
          "#include <stdio.h>\n#include <string.h>\n\nint main(void) {\n    char name[20] = \"Mal\";\n    printf(\"%d %d\\n\", (int)strlen(name), (int)sizeof(name));\n    return 0;\n}\n",
          "3 20", { "making a copy strlen(s) long: no room for the '\\0'", "calling it every time round a loop (slow)" } },
        { "strcpy", "strcpy", "string.h", "strings", "char *strcpy(char *dest, const char *src);",
          "Copies a string, '\\0' and all, into dest. dest must be big enough.",
          "#include <stdio.h>\n#include <string.h>\n\nint main(void) {\n    char a[10];\n    strcpy(a, \"heap\");\n    a[0] = 'H';\n    printf(\"%s\\n\", a);\n    return 0;\n}\n",
          "Heap", { "dest too small: it writes past the end", "a = \"heap\"; doesn't copy a string into an array" } },
        { "strcmp", "strcmp", "string.h", "strings", "int strcmp(const char *a, const char *b);",
          "Compares two strings: 0 if they're the same, below 0 if a comes first, above 0 if b does.",
          "#include <stdio.h>\n#include <string.h>\n\nint main(void) {\n    char pw[] = \"0x9000\";\n    if (strcmp(pw, \"0x9000\") == 0)\n        printf(\"match\\n\");\n    printf(\"%d\\n\", strcmp(\"ant\", \"bee\") < 0);\n    return 0;\n}\n",
          "match\n1", { "a == b compares the addresses, not the text", "if (strcmp(a, b)) is true when they're DIFFERENT" } },
        { "strcat", "strcat", "string.h", "strings", "char *strcat(char *dest, const char *src);",
          "Adds src onto the end of dest. dest needs room for both.",
          "#include <stdio.h>\n#include <string.h>\n\nint main(void) {\n    char s[20] = \"Stack\";\n    strcat(s, \"_\");\n    strcat(s, \"Trace\");\n    printf(\"%s\\n\", s);\n    return 0;\n}\n",
          "Stack_Trace", { "dest without room: it overflows", "dest not a string yet (no '\\0'): it starts in the wrong place" } },
        { "nul", "'\\0'", "char", "strings", "char s[] = \"hi\";   // 'h' 'i' '\\0'",
          "The null character that ends every C string. printf %s and strlen stop when they reach it.",
          "#include <stdio.h>\n\nint main(void) {\n    char s[] = \"pointer\";\n    s[5] = '\\0';\n    printf(\"%s\\n\", s);\n    printf(\"%d\\n\", s[7] == 0);\n    return 0;\n}\n",
          "point\n1", { "building a string by hand and forgetting the '\\0'", "'0' (the digit) is not '\\0'" } },
        { "function", "functions", "keyword", "functions", "type name(params) { ... return value; }",
          "A named piece of code. Arguments are copies (pass by value), so changing them doesn't change the caller's.",
          "#include <stdio.h>\n\nint twice(int x) {\n    x = x * 2;\n    return x;\n}\n\nint main(void) {\n    int a = 4;\n    int b = twice(a);\n    printf(\"%d %d\\n\", a, b);\n    return 0;\n}\n",
          "4 8", { "expecting the function to change the caller's variable", "using it before it's declared" } },

        // ----- Pointers and memory -----
        { "address", "& address-of", "operator", "pointers", "&variable",
          "Gives a variable's address: where it lives in memory. That's what a pointer holds.",
          "#include <stdio.h>\n\nint main(void) {\n    int x = 5;\n    int *p = &x;\n    printf(\"%d\\n\", p == &x);\n    printf(\"%d\\n\", *p);\n    return 0;\n}\n",
          "1\n5", { "& on a value: &5 makes no sense", "mixing up & (address) and * (follow it)" } },
        { "deref", "* dereference", "operator", "pointers", "*pointer",
          "Follows a pointer to what it points at. *p = 7 changes the thing p points to.",
          "#include <stdio.h>\n\nint main(void) {\n    int x = 5;\n    int *p = &x;\n    *p = 7;\n    printf(\"%d\\n\", x);\n    return 0;\n}\n",
          "7", { "*p when p is NULL or not set: a segfault", "p = 7 changes the pointer, not x" } },
        { "null", "NULL", "stddef.h", "pointers", "int *p = NULL;",
          "A pointer that points at nothing. Check for it before following a pointer.",
          "#include <stdio.h>\n#include <stddef.h>\n\nint main(void) {\n    int *p = NULL;\n    if (p == NULL)\n        printf(\"nothing there\\n\");\n    return 0;\n}\n",
          "nothing there", { "*p on NULL: an instant segfault", "leaving a pointer unset instead of NULL" } },
        { "arith", "p + n", "operator", "arith", "p + n   p++   p[n] == *(p + n)",
          "Adding n to a pointer moves it n elements along, not n bytes. p[i] is just *(p + i).",
          "#include <stdio.h>\n\nint main(void) {\n    int a[] = {10, 20, 30, 40};\n    int *p = a;\n    p += 2;\n    printf(\"%d %d\\n\", *p, p[-1]);\n    printf(\"%d\\n\", (int)(p - a));\n    return 0;\n}\n",
          "30 20\n2", { "walking off the end of the array", "thinking p + 1 is one byte on" } },
        { "passbyptr", "pass by pointer", "pattern", "passbyptr", "void f(int *x) { *x = ...; }   f(&a);",
          "Give a function an address and it can change the caller's variable through it.",
          "#include <stdio.h>\n\nvoid reset(int *x) {\n    *x = 0;\n}\n\nint main(void) {\n    int score = 99;\n    reset(&score);\n    printf(\"%d\\n\", score);\n    return 0;\n}\n",
          "0", { "f(a) instead of f(&a)", "x = 0 inside: changes the copy of the pointer, not the int" } },
        { "dblptr", "** double pointer", "type", "dblptr", "int **pp = &p;",
          "A pointer to a pointer. Lets a function change which thing a pointer points at.",
          "#include <stdio.h>\n\nint main(void) {\n    int a = 1, b = 2;\n    int *p = &a;\n    int **pp = &p;\n    *pp = &b;\n    printf(\"%d\\n\", *p);\n    return 0;\n}\n",
          "2", { "*pp vs **pp: one arrow or two", "passing p instead of &p to a function that sets it" } },
        { "malloc", "malloc", "stdlib.h", "dynmem", "void *malloc(size_t size);",
          "Asks the heap for size bytes. Returns a pointer to them, or NULL if there's no room. The memory stays until you free() it.",
          "#include <stdio.h>\n#include <stdlib.h>\n\nint main(void) {\n    int *p = malloc(3 * sizeof(int));\n    if (p == NULL) return 1;\n    p[0] = 7; p[1] = 4; p[2] = 9;\n    printf(\"%d\\n\", p[1]);\n    free(p);\n    return 0;\n}\n",
          "4", { "forgetting free(p): a leak", "malloc(3) instead of malloc(3 * sizeof(int))" } },
        { "calloc", "calloc", "stdlib.h", "dynmem", "void *calloc(size_t count, size_t size);",
          "Like malloc for count things of size bytes, but it sets them all to zero first.",
          "#include <stdio.h>\n#include <stdlib.h>\n\nint main(void) {\n    int *p = calloc(4, sizeof(int));\n    printf(\"%d %d\\n\", p[0], p[3]);\n    free(p);\n    return 0;\n}\n",
          "0 0", { "expecting malloc to zero memory (it doesn't)", "mixing the two arguments up (it still works, but reads badly)" } },
        { "realloc", "realloc", "stdlib.h", "dynmem", "void *realloc(void *p, size_t size);",
          "Makes a heap block bigger or smaller. It may move it, so use the pointer it returns.",
          "#include <stdio.h>\n#include <stdlib.h>\n\nint main(void) {\n    int *p = malloc(2 * sizeof(int));\n    p[0] = 1; p[1] = 2;\n    p = realloc(p, 3 * sizeof(int));\n    p[2] = 3;\n    printf(\"%d%d%d\\n\", p[0], p[1], p[2]);\n    free(p);\n    return 0;\n}\n",
          "123", { "keeping the old pointer: it may have moved", "p = realloc(p, ...) loses the block if it returns NULL" } },
        { "free", "free", "stdlib.h", "dynmem", "void free(void *p);",
          "Gives a heap block back. After that the pointer is dangling, so set it to NULL.",
          "#include <stdio.h>\n#include <stdlib.h>\n\nint main(void) {\n    char *s = malloc(8);\n    s[0] = 'M'; s[1] = '\\0';\n    printf(\"%s\\n\", s);\n    free(s);\n    s = NULL;\n    printf(\"%d\\n\", s == NULL);\n    return 0;\n}\n",
          "M\n1", { "using it after free: a dangling pointer", "freeing it twice: a double free" } },
        { "funcptr", "function pointer", "type", "funcptr", "int (*f)(int, int) = add;",
          "A pointer to a function. Call it like the function; pass it around to choose what code runs.",
          "#include <stdio.h>\n\nint add(int a, int b) { return a + b; }\nint sub(int a, int b) { return a - b; }\n\nint main(void) {\n    int (*op)(int, int) = add;\n    printf(\"%d\\n\", op(5, 3));\n    op = sub;\n    printf(\"%d\\n\", op(5, 3));\n    return 0;\n}\n",
          "8\n2", { "int *f(int) is a function returning a pointer: the brackets matter", "calling it with the wrong argument types" } },

        // ----- Linked lists -----
        { "struct", "struct", "keyword", "sll", "struct Node { int value; struct Node *next; };",
          "Groups values together into one type. A struct can hold a pointer to another of itself: that's a linked list node.",
          "#include <stdio.h>\n\nstruct Point { int x, y; };\n\nint main(void) {\n    struct Point p = {3, 4};\n    p.x += 10;\n    printf(\"(%d, %d)\\n\", p.x, p.y);\n    return 0;\n}\n",
          "(13, 4)", { "forgetting the ; after the closing }", "comparing two structs with ==" } },
        { "arrow", "->", "operator", "sll", "ptr->field  ==  (*ptr).field",
          "Gets a field through a pointer to a struct. Used all over linked lists.",
          "#include <stdio.h>\n\nstruct Node { int value; struct Node *next; };\n\nint main(void) {\n    struct Node b = {2, NULL};\n    struct Node a = {1, &b};\n    struct Node *n = &a;\n    printf(\"%d\\n\", n->next->value);\n    return 0;\n}\n",
          "2", { "n.value when n is a pointer", "n->next->value when next is NULL: a segfault" } },
        { "typedef", "typedef", "keyword", "sll", "typedef struct Node Node;",
          "Gives a type a new name, so you can write Node instead of struct Node.",
          "#include <stdio.h>\n\ntypedef struct { int hp; } Boss;\n\nint main(void) {\n    Boss leaker = {120};\n    leaker.hp -= 20;\n    printf(\"%d\\n\", leaker.hp);\n    return 0;\n}\n",
          "100", { "a self-pointing struct still needs its struct name inside", "hiding pointers in typedefs makes code hard to read" } },
        { "dll", "prev pointer", "pattern", "dll", "struct Node { int v; struct Node *prev, *next; };",
          "A doubly linked list node also points back. Every insert or delete fixes both directions.",
          "#include <stdio.h>\n\nstruct N { int v; struct N *prev, *next; };\n\nint main(void) {\n    struct N a = {1, NULL, NULL}, b = {2, NULL, NULL};\n    a.next = &b;\n    b.prev = &a;\n    printf(\"%d %d\\n\", a.next->v, b.prev->v);\n    return 0;\n}\n",
          "2 1", { "fixing next but forgetting prev", "not updating head when deleting the first node" } },

        // ----- C++ -----
        { "cout", "cout", "iostream", "classes", "std::cout << value << std::endl;",
          "C++'s printf. << sends values to the screen; it works out their types for you.",
          "#include <iostream>\nusing namespace std;\n\nint main() {\n    int lives = 3;\n    cout << \"lives: \" << lives << endl;\n    return 0;\n}\n",
          "lives: 3", { "forgetting using namespace std; (or std::)", "using >> (that's cin)" }, true },
        { "class", "class", "C++", "classes", "class Name { public: ... private: ... };",
          "Bundles data and the functions (methods) that work on it. Members are private unless you say public.",
          "#include <iostream>\nusing namespace std;\n\nclass Counter {\npublic:\n    int n = 0;\n    void tick() { n++; }\n};\n\nint main() {\n    Counter c;\n    c.tick(); c.tick();\n    cout << c.n << endl;\n    return 0;\n}\n",
          "2", { "forgetting the ; after the class's }", "using a private member from outside" }, true },
        { "ctor", "constructor", "C++", "ctors", "Name(params) : member(value) { }",
          "Runs when an object is made, to set it up. Same name as the class, no return type.",
          "#include <iostream>\nusing namespace std;\n\nclass Box {\npublic:\n    int size;\n    Box(int s) : size(s) { cout << \"made \" << s << endl; }\n};\n\nint main() {\n    Box b(4);\n    cout << b.size << endl;\n    return 0;\n}\n",
          "made 4\n4", { "giving it a return type (even void)", "Box b(); declares a function, not a Box" }, true },
        { "copyctor", "copy constructor", "C++", "copyctor", "Name(const Name &other)",
          "Runs when an object is copied. If the object owns heap memory, copy that memory too (a deep copy).",
          "#include <iostream>\nusing namespace std;\n\nclass Bag {\npublic:\n    int *items;\n    Bag() { items = new int[2]{1, 2}; }\n    Bag(const Bag &o) { items = new int[2]{o.items[0], o.items[1]}; }\n    ~Bag() { delete[] items; }\n};\n\nint main() {\n    Bag a;\n    Bag b = a;\n    b.items[0] = 9;\n    cout << a.items[0] << b.items[0] << endl;\n    return 0;\n}\n",
          "19", { "the default copy shares the pointer: a shallow copy", "taking the parameter by value (it'd copy forever)" }, true },
        { "access", "public / private", "C++", "access", "public:   private:   protected:",
          "private members can only be used by the class's own methods. That keeps the data safe from outside.",
          "#include <iostream>\nusing namespace std;\n\nclass Account {\n    int balance = 0;\npublic:\n    void add(int n) { if (n > 0) balance += n; }\n    int get() const { return balance; }\n};\n\nint main() {\n    Account a;\n    a.add(50); a.add(-10);\n    cout << a.get() << endl;\n    return 0;\n}\n",
          "50", { "making everything public", "a getter that isn't const" }, true },
        { "static", "static member", "C++", "access", "static int count;",
          "One copy shared by every object of the class, not one each.",
          "#include <iostream>\nusing namespace std;\n\nclass Mal {\npublic:\n    static int made;\n    Mal() { made++; }\n};\nint Mal::made = 0;\n\nint main() {\n    Mal a, b, c;\n    cout << Mal::made << endl;\n    return 0;\n}\n",
          "3", { "forgetting to define it outside the class", "thinking each object has its own" }, true },
        { "dtor", "destructor", "C++", "dtors", "~Name() { ... }",
          "Runs when an object is destroyed (it goes out of scope or is deleted). Free what the object owns here.",
          "#include <iostream>\nusing namespace std;\n\nclass Frame {\npublic:\n    int id;\n    Frame(int i) : id(i) {}\n    ~Frame() { cout << \"pop \" << id << endl; }\n};\n\nint main() {\n    Frame a(1);\n    {\n        Frame b(2);\n    }\n    cout << \"end\" << endl;\n    return 0;\n}\n",
          "pop 2\nend\npop 1", { "new in the constructor but no delete in the destructor: a leak", "calling it yourself" }, true },
        { "new", "new / delete", "C++", "objarrays", "T *p = new T(args);   delete p;",
          "C++'s malloc and free. new makes an object on the heap and runs its constructor; delete runs the destructor and frees it.",
          "#include <iostream>\nusing namespace std;\n\nint main() {\n    int *p = new int(42);\n    cout << *p << endl;\n    delete p;\n    p = nullptr;\n    return 0;\n}\n",
          "42", { "new with no delete: a leak", "mixing new with free(), or malloc with delete" }, true },
        { "newarr", "new[ ] / delete[ ]", "C++", "objarrays", "T *a = new T[n];   delete[] a;",
          "Makes an array on the heap. It must be freed with delete[], with the brackets.",
          "#include <iostream>\nusing namespace std;\n\nint main() {\n    int n = 4;\n    int *a = new int[n];\n    for (int i = 0; i < n; i++) a[i] = i * 10;\n    cout << a[3] << endl;\n    delete[] a;\n    return 0;\n}\n",
          "30", { "delete a; instead of delete[] a;", "using a after delete[]" }, true },
    };
    return list;
}

bool codex_unlocked(const Progress &p, const CodexEntry &e)
{
    return topic_mastery(p, e.topic) != Mastery::NotStarted;
}

int codex_stars(const Progress &p, const CodexEntry &e)
{
    switch (topic_mastery(p, e.topic)) {
        case Mastery::Mastered: return 3;
        case Mastery::Proficient: return 2;
        case Mastery::Familiar: return 1;
        default: return 0;
    }
}

int codex_found(const Progress &p)
{
    int n = 0;
    for (const CodexEntry &e : codex_entries()) if (codex_unlocked(p, e)) n++;
    return n;
}

static std::string lower(std::string s)
{
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<int> codex_search(const std::string &text)
{
    std::vector<int> hits;
    std::string want = lower(text);
    const std::vector<CodexEntry> &list = codex_entries();
    for (int i = 0; i < (int)list.size(); i++) {
        const CodexEntry &e = list[i];
        if (want.empty() || lower(e.name).find(want) != std::string::npos || lower(e.tag).find(want) != std::string::npos ||
            lower(e.topic).find(want) != std::string::npos || lower(e.id).find(want) != std::string::npos)
            hits.push_back(i);
    }
    return hits;
}
