// outputle.cpp - the puzzles and rules for Outputle (see outputle.h).

#include "outputle.h"

const std::vector<OutPuzzle> &outputle_puzzles()
{
    static const std::vector<OutPuzzle> list = {
        { { "int x = 5, *p = &x;", "*p += 2;", "printf(\"%d,%d\", x, x * x);" }, "7,49", "pointers",
          "p points at x, so *p += 2 changes x itself: x is 7, and 7 * 7 is 49." },
        { { "int a[] = {3, 1, 4, 1, 5};", "printf(\"%d-%d\", a[2], *(a + 4));" }, "4-5", "arith",
          "a[2] is the third element, 4. *(a + 4) is just another way to write a[4], which is 5." },
        { { "char s[] = \"hello\";", "printf(\"%d,%d\", (int)sizeof(s), (int)strlen(s));" }, "6,5", "strings",
          "sizeof counts the hidden '\\0' at the end (6 bytes), strlen stops before it (5 characters)." },
        { { "int i;", "for (i = 0; i < 5; i++);", "printf(\"i=%d\", i);" }, "i=5", "control",
          "The ; straight after the for is an empty body. The loop just counts i up to 5, then printf runs once." },
        { { "int x = 7 / 2;", "double y = 7 / 2.0;", "printf(\"%d,%.1f\", x, y);" }, "3,3.5", "basics",
          "7 / 2 is integer division, so the .5 is thrown away. 7 / 2.0 has a double in it, so it's 3.5." },
        { { "int n = 0;", "for (int i = 1; i <= 4; i++)", "    n += i;", "printf(\"n=%d\", n);" }, "n=10", "control",
          "1 + 2 + 3 + 4 = 10. The <= means 4 is included." },
        { { "int a[] = {10, 20, 30};", "int *p = a;", "p++;", "printf(\"%d,%d\", *p, *(p + 1));" }, "20,30", "arith",
          "p++ moves p one int along, to a[1]. *(p + 1) is one more along: a[2]." },
        { { "void f(int x) { x = 9; }", "void g(int *x) { *x = 9; }", "// in main:", "int a = 1, b = 1;", "f(a); g(&b);",
            "printf(\"%d,%d\", a, b);" }, "1,9", "passbyptr",
          "f gets a copy of a, so a doesn't change. g gets b's address and changes b through it." },
        { { "char s[] = \"abc\";", "s[1] = 'X';", "printf(\"%s\", s);" }, "aXc", "strings",
          "A char array can be changed one character at a time. s[1] is the 'b'." },
        { { "char s[] = \"hello\";", "s[2] = '\\0';", "printf(\"%s!\", s);" }, "he!", "strings",
          "printf's %s stops at the first '\\0'. Putting one at s[2] cuts the string to \"he\"." },
        { { "int x = 2;", "switch (x) {", "    case 1: printf(\"a\");", "    case 2: printf(\"b\");", "    case 3: printf(\"c\");",
            "    default: printf(\"d\");", "}" }, "bcd", "control",
          "No breaks! It jumps to case 2, then falls through every case after it." },
        { { "int x = 5;", "if (x = 0)", "    printf(\"yes\");", "else", "    printf(\"no%d\", x);" }, "no0", "control",
          "x = 0 is an assignment, not a comparison. It sets x to 0, and 0 is false, so the else runs." },
        { { "printf(\"%d,%d\", 17 / 5, 17 % 5);" }, "3,2", "basics",
          "17 / 5 is 3 (integer division) and 17 % 5 is the remainder, 2." },
        { { "int x = 1;", "int *p = &x;", "int **pp = &p;", "**pp = 8;", "printf(\"x=%d\", x);" }, "x=8", "dblptr",
          "pp points at p, which points at x. **pp follows both arrows, all the way to x." },
        { { "int f(int n) {", "    return n <= 1 ? 1 : n * f(n - 1);", "}", "// in main:", "printf(\"%d\", f(5));" }, "120",
          "functions", "f is factorial: 5 * 4 * 3 * 2 * 1 = 120." },
        { { "int n = 1;", "while (n < 100)", "    n *= 3;", "printf(\"%d\", n);" }, "243", "control",
          "1, 3, 9, 27, 81 are all under 100, so it goes once more: 243." },
        { { "struct P { int x, y; };", "struct P a = {2, 3};", "struct P *p = &a;", "p->y = 7;", "printf(\"%d,%d\", a.x, a.y);" },
          "2,7", "sll", "p->y is (*p).y: the y of the struct p points at, which is a." },
        { { "char c = 'a' + 2;", "printf(\"%c%c\", c, c - 32);" }, "cC", "basics",
          "Characters are numbers. 'a' + 2 is 'c', and upper case letters are 32 below lower case ones in ASCII." },
        { { "printf(\"%d,%c\", 'A', 66);" }, "65,B", "basics",
          "%d prints a char as its number (65 for 'A'); %c prints a number as its character (66 is 'B')." },
        { { "const char *w[] = {\"dog\", \"cat\", \"pig\"};", "printf(\"%c%c%c\", w[0][0], w[1][1], w[2][2]);" }, "dag", "arrays",
          "w[i][j] is character j of word i: the d of dog, the a of cat and the g of pig." },
        { { "int c = 0;", "for (int i = 0; i < 3; i++)", "    for (int j = i; j < 3; j++)", "        c++;", "printf(\"c=%d\", c);" },
          "c=6", "control", "The inner loop runs 3 times, then 2, then 1: 6 altogether." },
        { { "int a = 4, b = 9;", "printf(\"%d,%d\", a > b ? a : b, a < b ? a : b);" }, "9,4", "basics",
          "cond ? x : y picks x if cond is true. The first is the bigger one, the second the smaller." },
        { { "int *p = malloc(3 * sizeof(int));", "for (int i = 0; i < 3; i++)", "    p[i] = i * i;", "printf(\"%d%d%d\", p[0], p[1], p[2]);",
            "free(p);" }, "014", "dynmem", "malloc gives room for 3 ints, used like an array: 0*0, 1*1, 2*2." },
        { { "struct N { int v; struct N *next; };", "struct N c = {3, NULL}, b = {2, &c}, a = {1, &b};", "int s = 0;",
            "for (struct N *p = &a; p; p = p->next)", "    s = s * 10 + p->v;", "printf(\"%d\", s);" }, "123", "sll",
          "The loop walks the list a -> b -> c, adding each value as the next digit." },
        { { "char s[] = \"trace\";", "char *p = s + 3;", "printf(\"%s,%c\", p, *s);" }, "ce,t", "arith",
          "s + 3 points at the 'c', so %s prints from there: \"ce\". *s is the first character." },
        { { "void sw(int *a, int *b) {", "    int t = *a; *a = *b; *b = t;", "}", "// in main:", "int x = 3, y = 8;", "sw(&x, &y);",
            "printf(\"%d,%d\", x, y);" }, "8,3", "passbyptr", "sw gets the addresses, so it really swaps x and y." },
        { { "int i = 10;", "do {", "    i++;", "} while (i < 5);", "printf(\"%d\", i);" }, "11", "control",
          "A do-while always runs its body once before checking, so i becomes 11 even though 10 < 5 is false." },
        { { "printf(\"%d,%d\", (int)3.99, (int)-3.99);" }, "3,-3", "basics",
          "Casting to int cuts off the fraction (towards zero). It never rounds." },
        { { "int count(void) {", "    static int n = 0;", "    return ++n;", "}", "// in main:", "count(); count();",
            "printf(\"n=%d\", count());" }, "n=3", "functions",
          "A static local keeps its value between calls. It's set to 0 once, then goes 1, 2, 3." },
        { { "int add(int a, int b) { return a + b; }", "int mul(int a, int b) { return a * b; }", "// in main:",
            "int (*f)(int, int) = mul;", "printf(\"%d\", f(6, 7));" }, "42", "funcptr",
          "f is a pointer to a function. It points at mul, so f(6, 7) is 6 * 7." },
        { { "int g[2][3] = {{1, 2, 3}, {4, 5, 6}};", "printf(\"%d,%d\", g[1][0], g[0][2]);" }, "4,3", "arrays",
          "The first index is the row. g[1][0] starts the second row; g[0][2] ends the first." },
        { { "char s[] = \"memory\";", "int n = 0;", "while (s[n])", "    n++;", "printf(\"len=%d\", n);" }, "len=6", "strings",
          "while (s[n]) keeps going until it reaches the '\\0', which is 0 (false). That's how strlen works." },
        { { "int a[5] = {1, 2};", "printf(\"%d%d%d\", a[1], a[2], a[4]);" }, "200", "arrays",
          "Give an array fewer values than its size and the rest are set to 0." },
    };
    return list;
}

int outputle_number(const std::string &date)
{
    int d = date_to_days(date) - date_to_days(OUTPUTLE_START);
    return d + 1;
}

int outputle_index(const std::string &date)
{
    int n = (int)outputle_puzzles().size();
    int d = date_to_days(date) - date_to_days(OUTPUTLE_START);
    int i = (d * 7 + 3) % n;   // 7 and the count share no factor, so it goes through all of them
    return i < 0 ? i + n : i;
}

std::vector<int> outputle_grade(const std::string &guess, const std::string &answer)
{
    std::vector<int> colours(guess.size(), 0);
    std::string left;   // the answer's characters not matched green
    for (size_t i = 0; i < answer.size(); i++)
        if (i >= guess.size() || guess[i] != answer[i]) left += answer[i];
    for (size_t i = 0; i < guess.size(); i++) {
        if (i < answer.size() && guess[i] == answer[i]) { colours[i] = 2; continue; }
        size_t at = left.find(guess[i]);
        if (at != std::string::npos) { colours[i] = 1; left.erase(at, 1); }
    }
    return colours;
}

bool outputle_valid(const std::string &guess, const std::string &answer, std::string &why)
{
    if (guess.size() != answer.size()) {
        why = "It's " + std::to_string(answer.size()) + " characters long";
        return false;
    }
    for (char c : guess)
        if (c == ' ' || c == '|' || c < 33 || c > 126) { why = "No spaces"; return false; }
    return true;
}

const std::vector<std::string> &outputle_guesses(const Progress &p, const std::string &date)
{
    static const std::vector<std::string> none;
    return p.outputle.date == date ? p.outputle.guesses : none;
}

bool outputle_won(const Progress &p, const std::string &date)
{
    const std::vector<std::string> &g = outputle_guesses(p, date);
    return !g.empty() && g.back() == outputle_puzzles()[outputle_index(date)].answer;
}

bool outputle_over(const Progress &p, const std::string &date)
{
    return outputle_won(p, date) || (int)outputle_guesses(p, date).size() >= OUTPUTLE_TRIES;
}

int outputle_reward(int tries)
{
    return 10 + 5 * (OUTPUTLE_TRIES - tries);
}

bool outputle_guess(Progress &p, const std::string &date, const std::string &guess, int &bytesWon, std::string &why)
{
    bytesWon = 0;
    const OutPuzzle &puzzle = outputle_puzzles()[outputle_index(date)];
    if (outputle_over(p, date)) { why = "Come back tomorrow"; return false; }
    if (!outputle_valid(guess, puzzle.answer, why)) return false;
    if (p.outputle.date != date) { p.outputle.date = date; p.outputle.guesses.clear(); }
    p.outputle.guesses.push_back(guess);

    if (!outputle_over(p, date)) return true;
    OutputleStats &s = p.outputleStats;
    s.played++;
    if (outputle_won(p, date)) {
        int tries = (int)p.outputle.guesses.size();
        s.won++;
        s.dist[tries - 1]++;
        s.streak = (s.lastWin == date_add_days(date, -1)) ? s.streak + 1 : 1;
        if (s.streak > s.best) s.best = s.streak;
        s.lastWin = date;
        bytesWon = outputle_reward(tries);
        p.bytes += bytesWon;
    } else {
        s.streak = 0;
    }
    return true;
}

std::string outputle_share(const Progress &p, const std::string &date)
{
    const std::vector<std::string> &g = outputle_guesses(p, date);
    const std::string &answer = outputle_puzzles()[outputle_index(date)].answer;
    std::string out = "Outputle #" + std::to_string(outputle_number(date)) + " " +
                      (outputle_won(p, date) ? std::to_string(g.size()) : "X") + "/" + std::to_string(OUTPUTLE_TRIES) + "\n";
    for (const std::string &guess : g) {
        for (int c : outputle_grade(guess, answer)) out += c == 2 ? "\xF0\x9F\x9F\xA9" : c == 1 ? "\xF0\x9F\x9F\xA8" : "\xE2\xAC\x9B";
        out += "\n";
    }
    return out + "Stack Trace";
}
