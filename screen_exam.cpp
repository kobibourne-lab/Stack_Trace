// screen_exam.cpp - Exam prep, NeetCode style: the problems from the
// "Coding Exam 2 Preparation" lab in groups, each with a difficulty and a
// solved tick, and a card for the selected problem.
//
// Visuals only for now. Still to build:
//   - solving a problem (an editor, or a "fill the gap" version of one)
//   - the "no []" check: look for '[' in the student's answer
//   - exam mode: a timer, no hints
//   - remembering which problems are solved (in progress.txt)
//   - moving the problem list into a content file, like the lessons
//
// The prototypes below are our best reading of the lab sheet: check them
// against it before writing the real problems.

#include "app.h"
#include "theme.h"
#include "ui.h"

enum class Difficulty { Easy, Medium, Hard };

struct Problem {
    const char *name;
    const char *group;
    Difficulty difficulty;
    bool noBrackets;          // the lab's rule: pointer arithmetic only, no arr[i]
    const char *prototype;
    const char *task;
    std::vector<std::string> example;
};

static const std::vector<Problem> PROBLEMS = {
    { "swap_ints", "Swapping", Difficulty::Easy, true,
      "void swap_ints(int *a, int *b);",
      "Swap the two ints that a and b point to. The caller's variables must change.",
      { "int x = 3, y = 7;", "swap_ints(&x, &y);", "// x == 7, y == 3" } },
    { "swap_chars", "Swapping", Difficulty::Easy, true,
      "void swap_chars(char *a, char *b);",
      "Swap two chars through pointers, the same idea as swap_ints.",
      { "char a = 'A', b = 'Z';", "swap_chars(&a, &b);", "// a == 'Z', b == 'A'" } },

    { "sum_array", "Arrays", Difficulty::Easy, true,
      "int sum_array(int *arr, int size);",
      "Add up every element by moving a pointer along the array.",
      { "int a[] = {1, 2, 3, 4};", "sum_array(a, 4);   // 10" } },
    { "find_max", "Arrays", Difficulty::Easy, true,
      "int find_max(int *arr, int size);",
      "Return the largest element. Walk the array with a pointer, not an index.",
      { "int a[] = {4, 9, 2};", "find_max(a, 3);   // 9" } },
    { "reverse_array", "Arrays", Difficulty::Medium, true,
      "void reverse_array(int *arr, int size);",
      "Reverse the array in place, using one pointer at each end that move towards each other.",
      { "int a[] = {1, 2, 3, 4};", "reverse_array(a, 4);", "// a is now {4, 3, 2, 1}" } },

    { "strlen_ptr", "Strings", Difficulty::Easy, true,
      "int strlen_ptr(const char *str);",
      "Count the characters before the '\\0', without calling strlen.",
      { "strlen_ptr(\"hello\");   // 5" } },
    { "count_vowels", "Strings", Difficulty::Medium, true,
      "int count_vowels(const char *str);",
      "Count a, e, i, o and u (either case) by stepping through the string with a pointer.",
      { "count_vowels(\"Pointer\");   // 3" } },
    { "copy_string", "Strings", Difficulty::Medium, true,
      "void copy_string(char *dest, const char *src);",
      "Copy src into dest, including the '\\0' at the end.",
      { "char buf[16];", "copy_string(buf, \"exam\");", "// buf holds \"exam\"" } },
    { "concat_strings", "Strings", Difficulty::Medium, true,
      "void concat_strings(char *dest, const char *src);",
      "Find the end of dest, then copy src on after it.",
      { "char buf[16] = \"Stack\";", "concat_strings(buf, \"Trace\");", "// buf holds \"StackTrace\"" } },
    { "compare_strings", "Strings", Difficulty::Hard, true,
      "int compare_strings(const char *s1, const char *s2);",
      "Return 0 if the strings match, less than 0 if s1 comes first, more than 0 if s2 does (like strcmp).",
      { "compare_strings(\"abc\", \"abd\");   // < 0", "compare_strings(\"hi\", \"hi\");     // 0" } },

    { "insert_at_beginning", "Doubly linked list", Difficulty::Medium, false,
      "void insert_at_beginning(int value);",
      "Add a node at the head. Fix head, and the old first node's prev.",
      { "list: 2 <-> 3", "insert_at_beginning(1);", "list: 1 <-> 2 <-> 3" } },
    { "insert_at_end", "Doubly linked list", Difficulty::Medium, false,
      "void insert_at_end(int value);",
      "Add a node after the last one. Remember the empty list.",
      { "list: 1 <-> 2", "insert_at_end(3);", "list: 1 <-> 2 <-> 3" } },
    { "insert_at_position", "Doubly linked list", Difficulty::Hard, false,
      "void insert_at_position(int value, int pos);",
      "Insert so the new node ends up at pos (0 = head). Four pointers change.",
      { "list: 1 <-> 3", "insert_at_position(2, 1);", "list: 1 <-> 2 <-> 3" } },
};

const float LIST_W = 760;
const float ROW_H = 29;
const float GROUP_H = 32;

// A solid colour part way from the background to c. (A see-through fill would
// show ui_panel's border box underneath it.)
static Color tint(Color c, float amount)
{
    return { (unsigned char)(COL_BG.r + (c.r - COL_BG.r) * amount), (unsigned char)(COL_BG.g + (c.g - COL_BG.g) * amount),
             (unsigned char)(COL_BG.b + (c.b - COL_BG.b) * amount), 255 };
}

static const char *difficulty_name(Difficulty d)
{
    switch (d) {
        case Difficulty::Easy:   return "Easy";
        case Difficulty::Medium: return "Medium";
        default:                 return "Hard";
    }
}

static Color difficulty_color(Difficulty d)
{
    switch (d) {
        case Difficulty::Easy:   return COL_GREEN;
        case Difficulty::Medium: return COL_AMBER;
        default:                 return COL_CORAL;
    }
}

// A small rounded tag: filled, or just an outline.
static float draw_tag(const std::string &text, float x, float y, Color color, bool filled)
{
    float w = ui_text_width(text, 12, FontKind::Mono) + 14;
    if (filled) {
        DrawRectangleRounded({ x, y, w, 19 }, 0.3f, 4, color);
        ui_text(text, x + 7, y + 3, 12, COL_BG, FontKind::Mono);
    } else {
        ui_panel({ x, y, w, 19 }, COL_BG, tint(color, 0.6f), 1);
        ui_text(text, x + 7, y + 3, 12, color, FontKind::Mono);
    }
    return w;
}

static std::string two_digits(int n)
{
    return (n < 10 ? "0" : "") + std::to_string(n);
}

// Problems are never solved yet: this is where progress.txt will plug in.
static bool is_solved(const App &, const Problem &) { return false; }

static void draw_summary(const App &app, float x, float y)
{
    int solved = 0;
    for (const Problem &p : PROBLEMS) if (is_solved(app, p)) solved++;

    ui_text("Coding Exam 2 Preparation", x, y, 15, COL_TEXT, FontKind::UiBold);
    float tx = x + ui_text_width("Coding Exam 2 Preparation", 15, FontKind::UiBold) + 14;
    draw_tag("Exam 2", tx, y - 1, COL_PURPLE, true);

    std::string count = std::to_string(solved) + " / " + std::to_string(PROBLEMS.size()) + " solved";
    ui_text(count, x + LIST_W - ui_text_width(count, 13, FontKind::Mono), y + 2, 13, COL_TEXT_SOFT, FontKind::Mono);

    Rectangle bar = { x, y + 26, LIST_W, 8 };
    DrawRectangleRounded(bar, 1, 6, COL_LINE_SOFT);
    if (solved > 0) DrawRectangleRounded({ bar.x, bar.y, bar.width * solved / PROBLEMS.size(), bar.height }, 1, 6, COL_PURPLE);
}

static void draw_list(App &app, float x, float y)
{
    std::string group;
    for (int i = 0; i < (int)PROBLEMS.size(); i++) {
        const Problem &p = PROBLEMS[i];

        // A heading whenever the group changes, with its solved count
        if (group != p.group) {
            group = p.group;
            int total = 0, solved = 0;
            for (const Problem &q : PROBLEMS)
                if (group == q.group) { total++; if (is_solved(app, q)) solved++; }
            std::string upper = group;
            for (char &c : upper) c = (char)toupper((unsigned char)c);
            ui_text(upper, x, y + 10, 13, COL_TEXT, FontKind::MonoBold);
            if (!p.noBrackets) {
                float gx = x + ui_text_width(upper, 13, FontKind::MonoBold) + 12;
                ui_text("lab part 2", gx, y + 11, 12, COL_MUTED, FontKind::Mono);
            }
            std::string count = std::to_string(solved) + " / " + std::to_string(total);
            ui_text(count, x + LIST_W - ui_text_width(count, 12, FontKind::Mono), y + 11, 12, COL_MUTED, FontKind::Mono);
            y += GROUP_H;
        }

        Rectangle row = { x, y, LIST_W, ROW_H - 3 };
        bool selected = (i == app.examSelected);
        bool hover = CheckCollisionPointRec(GetMousePosition(), row);
        if (hover) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { app.examSelected = i; app.examMessage.clear(); }
        }
        if (selected) {
            ui_panel(row, COL_SURFACE_2, COL_LINE, 1);
            DrawRectangle((int)row.x, (int)row.y + 4, 3, (int)row.height - 8, COL_PURPLE);
        } else if (hover) {
            ui_panel(row, COL_SURFACE, COL_SURFACE, 0);
        }

        float cy = row.y + row.height / 2;

        // Solved tick, or an empty circle
        if (is_solved(app, p)) {
            DrawCircleV({ x + 20, cy }, 8, COL_GREEN);
            DrawLineEx({ x + 16, cy }, { x + 19, cy + 3 }, 2, COL_BG);
            DrawLineEx({ x + 19, cy + 3 }, { x + 24, cy - 3 }, 2, COL_BG);
        } else {
            DrawCircleLinesV({ x + 20, cy }, 7, COL_LINE);
        }

        ui_text(two_digits(i + 1), x + 40, cy - 8, 13, COL_MUTED, FontKind::Mono);
        ui_text(p.name, x + 72, cy - 9, 15, selected ? COL_TEXT : COL_TEXT_SOFT, FontKind::Mono);

        if (p.noBrackets) draw_tag("no []", x + 520, cy - 9.5f, COL_MUTED, false);
        ui_text(difficulty_name(p.difficulty), x + 610, cy - 8, 14, difficulty_color(p.difficulty), FontKind::UiBold);
        ui_text("--:--", x + LIST_W - 16 - ui_text_width("--:--", 13, FontKind::Mono), cy - 7, 13, COL_MUTED, FontKind::Mono);

        y += ROW_H;
    }
}

static void draw_problem_card(App &app, Rectangle r)
{
    const Problem &p = PROBLEMS[app.examSelected];
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    float x = r.x + 24, y = r.y + 24, w = r.width - 48;

    ui_text("PROBLEM " + two_digits(app.examSelected + 1) + "  |  " + p.group, x, y, 12, COL_MUTED, FontKind::Mono);
    y += 26;
    ui_text(p.name, x, y, 22, COL_TEXT, FontKind::MonoBold);
    y += 36;

    float tx = x;
    tx += draw_tag(difficulty_name(p.difficulty), tx, y, difficulty_color(p.difficulty), true) + 8;
    tx += draw_tag("Exam 2", tx, y, COL_PURPLE, false) + 8;
    if (p.noBrackets) draw_tag("pointers only", tx, y, COL_AMBER, false);
    y += 36;

    ui_text("Prototype", x, y, 13, COL_MUTED, FontKind::UiBold);
    y += 22;
    ui_code({ p.prototype }, { x, y, w, 44 }, 0, COL_PURPLE, false, 13);
    y += 58;

    ui_text("Task", x, y, 13, COL_MUTED, FontKind::UiBold);
    y += 22;
    y += ui_text_wrapped(p.task, x, y, w, 15, COL_TEXT_SOFT) + 14;

    ui_text("Example", x, y, 13, COL_MUTED, FontKind::UiBold);
    y += 22;
    float exH = 20 + p.example.size() * 13 * 1.55f;
    ui_code(p.example, { x, y, w, exH }, 0, COL_PURPLE, false, 13);
    y += exH + 16;

    if (p.noBrackets) {
        ui_panel({ x, y, w, 34 }, tint(COL_AMBER, 0.1f), tint(COL_AMBER, 0.5f), 1);
        ui_text("Rule: no arr[i]. Pointer arithmetic only.", x + 12, y + 9, 13, COL_AMBER, FontKind::Mono);
    }

    // Buttons along the bottom of the card
    float by = r.y + r.height - 24 - 46;
    float bw = (w - 12) / 2;
    if (ui_button("Solve", { x, by, bw, 46 }, ButtonStyle::Primary, COL_PURPLE))
        app.examMessage = "Solving isn't built yet. Next: an editor with the prototype filled in.";
    if (ui_button("Exam mode 15:00", { x + bw + 12, by, bw, 46 }, ButtonStyle::Outline, COL_TEXT))
        app.examMessage = "Exam mode isn't built yet: a 15 minute timer, no hints.";
    if (!app.examMessage.empty())
        ui_text_wrapped(app.examMessage, x, by - 46, w, 13, COL_AMBER, FontKind::Mono);
}

void screen_exam(App &app)
{
    int count = (int)PROBLEMS.size();
    if (app.examSelected < 0 || app.examSelected >= count) app.examSelected = 0;

    if (IsKeyPressed(KEY_ESCAPE)) { app.examMessage.clear(); open_map(app); return; }
    if (IsKeyPressed(KEY_DOWN)) { app.examSelected = (app.examSelected + 1) % count; app.examMessage.clear(); }
    if (IsKeyPressed(KEY_UP)) { app.examSelected = (app.examSelected + count - 1) % count; app.examMessage.clear(); }
    if (IsKeyPressed(KEY_P)) { app.examMessage.clear(); app.screen = Screen::Profile; return; }

    ui_header("Exam prep - practise the Coding Exam 2 lab, pointers only");
    if (ui_button("Back to the map", { SCREEN_W - MARGIN - 170, 12, 170, 40 }, ButtonStyle::Outline, COL_TEXT)) {
        app.examMessage.clear();
        open_map(app);
        return;
    }

    float x = MARGIN, y = HEADER_H + 22;
    draw_summary(app, x, y);
    draw_list(app, x, y + 44);

    draw_problem_card(app, { MARGIN + LIST_W + 24, HEADER_H + 22, SCREEN_W - 2 * MARGIN - LIST_W - 24,
                             SCREEN_H - HEADER_H - FOOTER_H - 44 });

    ui_footer_hints({ "UP DOWN choose problem", "P profile", "ESC topic map" });
}
