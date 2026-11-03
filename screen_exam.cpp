// screen_exam.cpp - Exam prep, NeetCode style: one tab per problem set (from
// content/practice/problems.txt), the problems in groups with a difficulty,
// a solved tick and the best time, and a card for the selected problem.
// Solve and Exam mode open it on the problem screen (screen_problem.cpp).
//
// Kobi's layout and styling; the problems now come from content files.

#include "app.h"
#include "theme.h"
#include "ui.h"

const float LIST_W = 760;
const float ROW_H = 29;
const float GROUP_H = 32;
const float TABS_H = 40;

// A solid colour part way from the background to c. (A see-through fill would
// show ui_panel's border box underneath it.)
static Color tint(Color c, float amount)
{
    return { (unsigned char)(COL_BG.r + (c.r - COL_BG.r) * amount), (unsigned char)(COL_BG.g + (c.g - COL_BG.g) * amount),
             (unsigned char)(COL_BG.b + (c.b - COL_BG.b) * amount), 255 };
}

static Color difficulty_color(const std::string &d)
{
    if (d == "Easy") return COL_GREEN;
    if (d == "Hard") return COL_CORAL;
    return COL_AMBER;
}

static Color set_color(const ProblemSet &s) { return s.exam == "Exam 2" ? COL_PURPLE : COL_BLUE; }

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

static std::string best_time(const App &app, const Problem &p)
{
    auto it = app.progress.solved.find(p.id);
    if (it == app.progress.solved.end() || it->second.seconds <= 0) return "--:--";
    int s = it->second.seconds;
    if (s >= 100 * 60) return "99:59";
    return two_digits(s / 60) + ":" + two_digits(s % 60);
}

// The set tabs, with each one's solved count
static void draw_tabs(App &app, float x, float y)
{
    for (int i = 0; i < (int)app.practice.size(); i++) {
        const ProblemSet &s = app.practice[i];
        int solved = 0;
        for (const Problem &p : s.problems) if (progress_is_solved(app.progress, p.id)) solved++;
        std::string label = s.title + "  " + std::to_string(solved) + "/" + std::to_string(s.problems.size());
        float w = ui_text_width(label, 14, FontKind::UiBold) + 32;
        bool on = (i == app.examSet);
        Rectangle r = { x, y, w, 32 };
        bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        if (hover && !on) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { app.examSet = i; app.examSelected = 0; app.examScroll = 0; app.examMessage.clear(); }
        }
        ui_panel(r, on ? tint(set_color(s), 0.18f) : (hover ? COL_SURFACE : COL_BG), on ? set_color(s) : COL_LINE, 1);
        ui_text(label, x + 16, y + 7, 14, on ? set_color(s) : COL_TEXT_SOFT, FontKind::UiBold);
        x += w + 10;
    }
}

static void draw_summary(const App &app, const ProblemSet &set, float x, float y)
{
    int solved = 0;
    for (const Problem &p : set.problems) if (progress_is_solved(app.progress, p.id)) solved++;

    std::string count = std::to_string(solved) + " / " + std::to_string(set.problems.size()) + " solved";
    ui_text(count, x + LIST_W - ui_text_width(count, 13, FontKind::Mono), y - 22, 13, COL_TEXT_SOFT, FontKind::Mono);

    Rectangle bar = { x, y, LIST_W, 6 };
    DrawRectangleRounded(bar, 1, 6, COL_LINE_SOFT);
    if (solved > 0) DrawRectangleRounded({ bar.x, bar.y, bar.width * solved / set.problems.size(), bar.height }, 1, 6, set_color(set));
}

// The height of the whole list, so it can scroll
static float list_height(const ProblemSet &set)
{
    float h = 0;
    std::string group;
    for (const Problem &p : set.problems) {
        if (group != p.group) { group = p.group; h += GROUP_H; }
        h += ROW_H;
    }
    return h;
}

// Where row i sits in the list (for keeping the selection on screen)
static float row_top(const ProblemSet &set, int index)
{
    float h = 0;
    std::string group;
    for (int i = 0; i <= index && i < (int)set.problems.size(); i++) {
        if (group != set.problems[i].group) { group = set.problems[i].group; h += GROUP_H; }
        if (i < index) h += ROW_H;
    }
    return h;
}

static void draw_list(App &app, const ProblemSet &set, Rectangle area)
{
    // Scroll with the wheel
    float maxScroll = list_height(set) - area.height;
    if (maxScroll < 0) maxScroll = 0;
    if (CheckCollisionPointRec(GetMousePosition(), area)) app.examScroll -= GetMouseWheelMove() * ROW_H * 2;
    if (app.examScroll > maxScroll) app.examScroll = maxScroll;
    if (app.examScroll < 0) app.examScroll = 0;

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width + 4, (int)area.height);
    float x = area.x, y = area.y - app.examScroll;
    std::string group;
    for (int i = 0; i < (int)set.problems.size(); i++) {
        const Problem &p = set.problems[i];

        // A heading whenever the group changes, with its solved count
        if (group != p.group) {
            group = p.group;
            int total = 0, solved = 0;
            for (const Problem &q : set.problems)
                if (group == q.group) { total++; if (progress_is_solved(app.progress, q.id)) solved++; }
            std::string upper = group;
            for (char &c : upper) c = (char)toupper((unsigned char)c);
            ui_text(upper, x, y + 10, 13, COL_TEXT, FontKind::MonoBold);
            std::string count = std::to_string(solved) + " / " + std::to_string(total);
            ui_text(count, x + LIST_W - ui_text_width(count, 12, FontKind::Mono), y + 11, 12, COL_MUTED, FontKind::Mono);
            y += GROUP_H;
        }

        Rectangle row = { x, y, LIST_W, ROW_H - 3 };
        bool visible = row.y + row.height > area.y && row.y < area.y + area.height;
        bool selected = (i == app.examSelected);
        bool hover = visible && CheckCollisionPointRec(GetMousePosition(), row) && CheckCollisionPointRec(GetMousePosition(), area);
        if (hover) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // Clicking the selected row again opens it
                if (selected) open_problem(app, app.examSet, i, false);
                app.examSelected = i;
                app.examMessage.clear();
            }
        }
        if (selected) {
            ui_panel(row, COL_SURFACE_2, COL_LINE, 1);
            DrawRectangle((int)row.x, (int)row.y + 4, 3, (int)row.height - 8, set_color(set));
        } else if (hover) {
            ui_panel(row, COL_SURFACE, COL_SURFACE, 0);
        }

        float cy = row.y + row.height / 2;
        bool solved = progress_is_solved(app.progress, p.id);
        if (solved) {
            DrawCircleV({ x + 20, cy }, 8, COL_GREEN);
            DrawLineEx({ x + 16, cy }, { x + 19, cy + 3 }, 2, COL_BG);
            DrawLineEx({ x + 19, cy + 3 }, { x + 24, cy - 3 }, 2, COL_BG);
        } else {
            DrawCircleLinesV({ x + 20, cy }, 7, COL_LINE);
        }

        ui_text(two_digits(i + 1), x + 40, cy - 8, 13, COL_MUTED, FontKind::Mono);
        ui_text(p.function, x + 72, cy - 9, 15, selected ? COL_TEXT : COL_TEXT_SOFT, FontKind::Mono);

        if (forbids_token(p, "[")) draw_tag("no []", x + 520, cy - 9.5f, COL_MUTED, false);
        else if (!p.forbids.empty()) draw_tag("rules", x + 520, cy - 9.5f, COL_MUTED, false);
        ui_text(p.difficulty, x + 610, cy - 8, 14, difficulty_color(p.difficulty), FontKind::UiBold);
        std::string t = best_time(app, p);
        ui_text(t, x + LIST_W - 16 - ui_text_width(t, 13, FontKind::Mono), cy - 7, 13, solved ? COL_GREEN : COL_MUTED, FontKind::Mono);

        y += ROW_H;
    }
    EndScissorMode();

    // A fade at the bottom when there's more below
    if (app.examScroll < maxScroll - 1) {
        for (int i = 0; i < 24; i++)
            DrawRectangle((int)area.x, (int)(area.y + area.height - 24 + i), (int)LIST_W, 1, Fade(COL_BG, i / 24.0f));
    }
}

static void draw_problem_card(App &app, const ProblemSet &set, Rectangle r)
{
    const Problem &p = set.problems[app.examSelected];
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    float x = r.x + 24, y = r.y + 24, w = r.width - 48;

    ui_text("PROBLEM " + two_digits(app.examSelected + 1) + "  |  " + p.group, x, y, 12, COL_MUTED, FontKind::Mono);
    y += 26;
    y += ui_text_wrapped(p.title, x, y, w, 22, COL_TEXT, FontKind::UiBold) + 2;
    ui_text(p.function, x, y, 13, COL_MUTED, FontKind::Mono);
    y += 28;

    float tx = x;
    tx += draw_tag(p.difficulty, tx, y, difficulty_color(p.difficulty), true) + 8;
    tx += draw_tag(set.exam.empty() ? set.title : set.exam, tx, y, set_color(set), false) + 8;
    if (forbids_token(p, "[")) tx += draw_tag("pointers only", tx, y, COL_AMBER, false) + 8;
    if (progress_is_solved(app.progress, p.id)) draw_tag("Solved", tx, y, COL_GREEN, false);
    y += 34;

    ui_text("Prototype", x, y, 13, COL_MUTED, FontKind::UiBold);
    y += 22;
    float protoH = ui_text_wrapped_height(p.prototype, w - 24, 13, FontKind::Mono) + 18;
    ui_panel({ x, y, w, protoH }, COL_BG, COL_LINE, 1);
    ui_text_wrapped(p.prototype, x + 12, y + 9, w - 24, 13, COL_TEXT_SOFT, FontKind::Mono);
    y += protoH + 14;

    ui_text("Task", x, y, 13, COL_MUTED, FontKind::UiBold);
    y += 22;
    if (!p.text.empty()) y += ui_text_wrapped(p.text[0], x, y, w, 15, COL_TEXT_SOFT) + 14;

    if (!p.example.empty() && y < r.y + r.height - 220) {
        ui_text("Example", x, y, 13, COL_MUTED, FontKind::UiBold);
        y += 22;
        float exH = 20 + p.example.size() * 13 * 1.55f;
        ui_code(p.example, { x, y, w, exH }, 0, set_color(set), false, 13);
        y += exH + 16;
    }

    if (forbids_token(p, "[") && y < r.y + r.height - 130) {
        ui_panel({ x, y, w, 34 }, tint(COL_AMBER, 0.1f), tint(COL_AMBER, 0.5f), 1);
        ui_text("Rule: no arr[i]. Pointer arithmetic only.", x + 12, y + 9, 13, COL_AMBER, FontKind::Mono);
    }

    // Buttons along the bottom of the card
    float by = r.y + r.height - 24 - 46;
    float bw = (w - 12) / 2;
    if (ui_button("Solve", { x, by, bw, 46 }, ButtonStyle::Primary, set_color(set)))
        open_problem(app, app.examSet, app.examSelected, false);
    std::string examLabel = "Exam mode " + two_digits(p.examMinutes) + ":00";
    if (ui_button(examLabel, { x + bw + 12, by, bw, 46 }, ButtonStyle::Outline, COL_TEXT))
        open_problem(app, app.examSet, app.examSelected, true);
    if (!app.examMessage.empty())
        ui_text_wrapped(app.examMessage, x, by - 46, w, 13, COL_AMBER, FontKind::Mono);
}

void screen_exam(App &app)
{
    if (IsKeyPressed(KEY_ESCAPE)) { app.examMessage.clear(); open_map(app); return; }
    if (IsKeyPressed(KEY_P)) { app.examMessage.clear(); app.screen = Screen::Profile; return; }

    ui_header("Exam prep - practise the labs, LeetCode style");
    if (ui_button("Back to the map", { SCREEN_W - MARGIN - 170, 12, 170, 40 }, ButtonStyle::Outline, COL_TEXT)) {
        app.examMessage.clear();
        open_map(app);
        return;
    }

    if (app.practice.empty()) {
        ui_text("No problems loaded", MARGIN, HEADER_H + 40, 24, COL_CORAL, FontKind::UiBold);
        ui_text_wrapped(app.practiceError, MARGIN, HEADER_H + 80, 900, 15, COL_TEXT_SOFT, FontKind::Mono);
        ui_footer_hints({ "ESC topic map" });
        return;
    }
    if (app.examSet >= (int)app.practice.size()) app.examSet = 0;
    const ProblemSet &set = app.practice[app.examSet];
    int count = (int)set.problems.size();
    if (app.examSelected < 0 || app.examSelected >= count) app.examSelected = 0;

    // Keys: up/down choose, left/right change tab, Enter solves, X exam mode
    bool moved = false;
    if (IsKeyPressed(KEY_DOWN)) { app.examSelected = (app.examSelected + 1) % count; moved = true; }
    if (IsKeyPressed(KEY_UP)) { app.examSelected = (app.examSelected + count - 1) % count; moved = true; }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT)) {
        int n = (int)app.practice.size();
        app.examSet = (app.examSet + (IsKeyPressed(KEY_RIGHT) ? 1 : n - 1)) % n;
        app.examSelected = 0;
        app.examScroll = 0;
        return;
    }
    if (IsKeyPressed(KEY_ENTER)) { open_problem(app, app.examSet, app.examSelected, false); return; }
    if (IsKeyPressed(KEY_X)) { open_problem(app, app.examSet, app.examSelected, true); return; }

    float x = MARGIN, y = HEADER_H + 18;
    draw_tabs(app, x, y);
    y += TABS_H + 18;
    draw_summary(app, set, x, y);
    y += 12;

    Rectangle listArea = { x, y, LIST_W, SCREEN_H - FOOTER_H - 12 - y };
    if (moved) {
        // Keep the selected row on screen
        float top = row_top(set, app.examSelected);
        if (top < app.examScroll) app.examScroll = top - GROUP_H;
        if (top + ROW_H > app.examScroll + listArea.height) app.examScroll = top + ROW_H - listArea.height;
    }
    draw_list(app, set, listArea);

    if (app.screen != Screen::ExamPrep) return;   // a click opened a problem
    draw_problem_card(app, set, { MARGIN + LIST_W + 24, HEADER_H + 18, SCREEN_W - 2 * MARGIN - LIST_W - 24,
                                  SCREEN_H - HEADER_H - FOOTER_H - 30 });

    if (!app.practiceError.empty())
        ui_text("Some problems didn't load: " + app.practiceError, MARGIN, SCREEN_H - FOOTER_H - 4, 11, COL_CORAL, FontKind::Mono);
    ui_footer_hints({ "UP DOWN choose", "LEFT RIGHT change set", "ENTER solve", "X exam mode", "P profile", "ESC topic map" });
}
