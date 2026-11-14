// bugdex.cpp - the bugs, their quick fixes, and catching them (see bugdex.h).

#include "bugdex.h"

const std::vector<Bug> &bugdex_bugs()
{
    static const std::vector<Bug> list = {
        { "offbyone", "Off-By-One", "array type", "<= where < belongs, and arr[size]", "the last index is size - 1",
          "for (int i = 0; i <= n; i++)", "for (int i = 0; i < n; i++)", "offbyone", BugShape::Block, 0xFF7B72, {
            { { "int a[5] = {1, 2, 3, 4, 5};", "int sum = 0;", "for (int i = 0; i <= 5; i++)", "    sum += a[i];" }, 3,
              "i <= 5 reads a[5], one past the end. It should be i < 5." },
            { { "char s[4];", "for (int i = 0; i < 4; i++)", "    s[i] = 'x';", "s[4] = '\\0';" }, 4,
              "s has 4 places, s[0] to s[3]. s[4] is outside it: make s 5 long." },
            { { "// print 1 to 10", "for (int i = 1; i < 10; i++)", "    printf(\"%d \", i);" }, 2,
              "i < 10 stops at 9. To include 10 it needs i <= 10." } } },
        { "segfault", "Segfault", "pointer type", "NULL pointers and pointers never set", "check for NULL before you follow it",
          "*p = 5;   // p is NULL", "if (p != NULL) *p = 5;", "", BugShape::Bolt, 0xFFB454, {
            { { "int *p = NULL;", "int x = 3;", "*p = x;", "printf(\"%d\", *p);" }, 3,
              "p points at nothing, so *p = x writes to address 0. Point it at something first: p = &x." },
            { { "struct Node *head = NULL;", "// the list is empty", "printf(\"%d\", head->value);" }, 3,
              "head is NULL when the list is empty. Check if (head != NULL) first." },
            { { "int *p;", "*p = 10;", "printf(\"%d\", *p);" }, 2,
              "p was never set, so it points somewhere random. Give it an address or malloc first." } } },
        { "leak", "Memory Leak", "heap type", "malloc with no free, and lost pointers", "every malloc needs one free",
          "p = malloc(100);   // ...and never freed", "free(p);", "leaker", BugShape::Drop, 0x7EE787, {
            { { "for (int i = 0; i < 10; i++) {", "    int *p = malloc(sizeof(int));", "    *p = i;", "}" }, 2,
              "Each go round mallocs a new int and loses it. free(p) at the end of the loop body." },
            { { "char *s = malloc(20);", "strcpy(s, \"hi\");", "s = malloc(40);", "free(s);" }, 3,
              "Mallocing into s again loses the first 20 bytes for good. free(s) before reusing it." },
            { { "struct Node *n = head;", "head = NULL;", "// the whole list is gone", "return;" }, 2,
              "Setting head to NULL doesn't free the nodes, it just loses them. Free each node first." } } },
        { "dangling", "Dangling Pointer", "ghost type", "freed memory and returned local arrays", "set it to NULL after free",
          "free(p); *p = 1;", "free(p); p = NULL;", "dangling", BugShape::Ghost, 0x79C0FF, {
            { { "int *p = malloc(sizeof(int));", "*p = 4;", "free(p);", "printf(\"%d\", *p);" }, 4,
              "p was freed on line 3, so *p reads memory that isn't yours any more." },
            { { "int *make(void) {", "    int a[3] = {1, 2, 3};", "    return a;", "}" }, 3,
              "a lives in make's stack frame, which is gone once make returns. malloc it instead." },
            { { "char *name = strdup(\"Mal\");", "char *alias = name;", "free(name);", "printf(\"%s\", alias);" }, 4,
              "alias points at the same block as name, and it was freed. Both pointers are dangling now." } } },
        { "garbage", "Garbage Value", "variable type", "variables used before they're given a value",
          "give every variable a value when you make it", "int total;  total += x;", "int total = 0;", "", BugShape::Block, 0x8B949E, {
            { { "int total;", "for (int i = 0; i < 3; i++)", "    total += i;", "printf(\"%d\", total);" }, 1,
              "total starts with whatever was in memory. It needs int total = 0;" },
            { { "int max;", "int a[3] = {4, 9, 2};", "for (int i = 0; i < 3; i++)", "    if (a[i] > max) max = a[i];" }, 1,
              "max starts as rubbish, maybe bigger than everything. Start it at a[0]." },
            { { "int count;", "if (found)", "    count = 1;", "printf(\"%d\", count);" }, 1,
              "If found is false, count is never set before it's printed. Start it at 0." } } },
        { "infloop", "Infinite Loop", "loop type", "loops whose condition never changes", "make sure every loop moves towards its end",
          "while (i < n) { sum += i; }", "while (i < n) { sum += i; i++; }", "", BugShape::Loop, 0xD2A8FF, {
            { { "int i = 0;", "while (i < 10) {", "    printf(\"%d\", i);", "}" }, 2,
              "Nothing changes i, so i < 10 is true forever. Add i++ in the body." },
            { { "for (int i = 10; i > 0; i++)", "    printf(\"%d\", i);" }, 1,
              "Counting down needs i--. i++ makes i bigger, so i > 0 never ends." },
            { { "struct Node *p = head;", "while (p != NULL) {", "    printf(\"%d\", p->value);", "}" }, 2,
              "p never moves on. The loop needs p = p->next; at the end." } } },
        { "noterm", "Missing \\0", "string type", "strings built by hand", "always end a string with '\\0'",
          "char s[3] = {'h', 'i', '!'};", "char s[4] = {'h', 'i', '!', '\\0'};", "", BugShape::Blob, 0x56D4DD, {
            { { "char s[2];", "s[0] = 'o';", "s[1] = 'k';", "printf(\"%s\", s);" }, 1,
              "There's no room for the '\\0', so printf keeps reading past the end. Make it s[3] and set s[2] = '\\0'." },
            { { "char copy[5];", "for (int i = 0; i < 5; i++)", "    copy[i] = src[i];", "printf(\"%s\", copy);" }, 1,
              "\"hello\" needs 6 chars with its '\\0'. copy is too small to end the string." },
            { { "char word[10];", "word[0] = 'h';", "word[1] = 'i';", "printf(\"%s\", word);" }, 4,
              "word was never ended, so printf prints rubbish after \"hi\". Set word[2] = '\\0' first." } } },
        { "assign", "= vs ==", "condition type", "if and while conditions", "== compares, = assigns",
          "if (x = 5)", "if (x == 5)", "", BugShape::Bolt, 0xFFB454, {
            { { "int lives = 3;", "if (lives = 0)", "    printf(\"game over\");" }, 2,
              "lives = 0 sets lives to 0. To compare, write lives == 0." },
            { { "char c = getchar();", "while (c = 'y') {", "    c = getchar();", "}" }, 2,
              "c = 'y' is always true (it's not 0). The loop needs c == 'y'." },
            { { "int *p = find(list, 7);", "if (p = NULL)", "    printf(\"not found\");" }, 2,
              "p = NULL throws away what find returned. Compare with p == NULL." } } },
        { "fallthrough", "Fall-Through", "switch type", "switch cases with no break", "end each case with break",
          "case 1: printf(\"one\");", "case 1: printf(\"one\"); break;", "", BugShape::Drop, 0xFF7B72, {
            { { "switch (choice) {", "    case 1: printf(\"start\");", "    case 2: printf(\"quit\"); break;", "}" }, 2,
              "Choosing 1 prints start and then quit too. Case 1 needs a break." },
            { { "switch (grade) {", "    case 'A': points = 4; break;", "    case 'B': points = 3;", "    default: points = 0;", "}" }, 3,
              "A B falls into default and ends up 0. Add break after points = 3." },
            { { "switch (cmd) {", "    case 'q': running = 0;", "    case 'h': help();", "}" }, 2,
              "Quitting shows the help as well. Case 'q' needs a break." } } },
        { "doublefree", "Double Free", "heap type", "two frees of the same block", "free once, then set it to NULL",
          "free(p); free(p);", "free(p); p = NULL;", "", BugShape::Ghost, 0x7EE787, {
            { { "int *p = malloc(8);", "free(p);", "free(p);" }, 3,
              "p was already freed. Freeing it again can crash. Set p = NULL after the first free." },
            { { "char *a = malloc(4);", "char *b = a;", "free(a);", "free(b);" }, 4,
              "b is the same block as a. It was freed on line 3." },
            { { "void cleanup(int *p) { free(p); }", "int *data = malloc(40);", "cleanup(data);", "free(data);" }, 4,
              "cleanup already freed data. The last free is the second one." } } },
        { "shallow", "Shallow Copy", "object type", "copying objects that own heap memory", "write a copy constructor that copies the memory",
          "Bag b = a;   // shares a's array", "Bag(const Bag &o) { items = new int[n]; ... }", "shallow", BugShape::Blob, 0xD2A8FF, {
            { { "class Bag {", "    int *items;", "public:", "    Bag() { items = new int[3]; }", "    ~Bag() { delete[] items; }", "};" }, 4,
              "No copy constructor, so copies share items and both delete it. Add Bag(const Bag &o) that makes a new array." },
            { { "Bag(const Bag &o) {", "    size = o.size;", "    items = o.items;", "}" }, 3,
              "items = o.items shares the array. Make a new one and copy each element across." },
            { { "Bag a;", "Bag b = a;", "b.items[0] = 9;", "// a.items[0] is 9 too!" }, 2,
              "Bag b = a uses the default copy, which copies the pointer, not the array." } } },
        { "intdiv", "Integer Division", "maths type", "dividing two ints", "make one of them a double",
          "double avg = total / count;", "double avg = (double)total / count;", "", BugShape::Block, 0x79C0FF, {
            { { "int total = 7, count = 2;", "double avg = total / count;", "printf(\"%.1f\", avg);" }, 2,
              "7 / 2 is done with ints, giving 3, before it becomes a double. Cast one: (double)total / count." },
            { { "int done = 3, all = 4;", "int percent = done / all * 100;", "printf(\"%d%%\", percent);" }, 2,
              "3 / 4 is 0 in integer maths, so it's 0%. Multiply first: done * 100 / all." },
            { { "double half = 1 / 2;", "printf(\"%f\", half);" }, 1,
              "1 / 2 is 0 with ints. Write 1.0 / 2." } } },
        { "noamp", "Missing &", "scanf type", "scanf calls", "scanf needs the variable's address",
          "scanf(\"%d\", n);", "scanf(\"%d\", &n);", "", BugShape::Drop, 0xFFB454, {
            { { "int age;", "printf(\"Age? \");", "scanf(\"%d\", age);" }, 3,
              "scanf needs to know where age lives: &age." },
            { { "double price;", "scanf(\"%lf\", price);", "printf(\"%.2f\", price);" }, 2,
              "price should be &price, or scanf writes to a random address." },
            { { "int a, b;", "scanf(\"%d %d\", &a, b);" }, 2,
              "a has its &, but b doesn't." } } },
        { "stackoverflow", "Stack Overflow", "recursion type", "recursion with no base case", "every recursive function needs a way to stop",
          "int f(int n) { return n * f(n - 1); }", "int f(int n) { if (n <= 1) return 1; return n * f(n - 1); }", "", BugShape::Loop, 0xFF7B72, {
            { { "int fact(int n) {", "    return n * fact(n - 1);", "}" }, 2,
              "It never stops calling itself, so the stack fills up. Add if (n <= 1) return 1; first." },
            { { "int count(struct Node *p) {", "    return 1 + count(p->next);", "}" }, 2,
              "At the end of the list p is NULL, but it keeps going. Add if (p == NULL) return 0;" },
            { { "int sum(int n) {", "    if (n == 0) return 0;", "    return n + sum(n + 1);", "}" }, 3,
              "n + 1 goes away from the base case. It should be sum(n - 1)." } } },
    };
    return list;
}

const Bug *bugdex_find(const std::string &id)
{
    for (const Bug &b : bugdex_bugs()) if (b.id == id) return &b;
    return nullptr;
}

bool bug_caught(const Progress &p, const std::string &id)
{
    auto it = p.bugs.find(id);
    return it != p.bugs.end() && it->second.caught > 0;
}

int bugs_caught(const Progress &p)
{
    int n = 0;
    for (const Bug &b : bugdex_bugs()) if (bug_caught(p, b.id)) n++;
    return n;
}

bool bugdex_catch(Progress &p, const std::string &id, const std::string &date, const std::string &how)
{
    if (!bugdex_find(id)) return false;
    BugRecord &r = p.bugs[id];
    bool first = r.caught == 0;
    r.caught++;
    if (first) { r.date = date; r.how = how; }
    return first;
}

bool bugdex_fix_done(const Progress &p, const std::string &id, int fix)
{
    auto it = p.bugFixes.find(id);
    return it != p.bugFixes.end() && (it->second & (1 << fix)) != 0;
}

bool bugdex_answer_fix(Progress &p, const std::string &id, int fix, int line, const std::string &date, bool &caughtNow)
{
    caughtNow = false;
    const Bug *b = bugdex_find(id);
    if (!b || fix < 0 || fix >= (int)b->fixes.size()) return false;
    if (line != b->fixes[fix].line) return false;
    int &mask = p.bugFixes[id];
    bool wasAll = mask == (1 << (int)b->fixes.size()) - 1;
    mask |= 1 << fix;
    if (!wasAll && mask == (1 << (int)b->fixes.size()) - 1) {
        bugdex_catch(p, id, date, "practice");
        caughtNow = true;
    }
    return true;
}

std::string bugdex_for_boss(const std::string &bossId)
{
    for (const Bug &b : bugdex_bugs()) if (!b.boss.empty() && b.boss == bossId) return b.id;
    return "";
}
