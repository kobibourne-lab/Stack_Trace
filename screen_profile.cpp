// screen_profile.cpp - the Profile, GitHub style: who you are and your stats on
// the left, then pinned topics, a heatmap of answers per day for the last year,
// and your most recent answers.
//
// The stats, pinned topics, heatmap and recent activity are real: they come
// from progress.txt. Still to build:
//   - a name you choose (for now it's your computer's user name)
//   - Edit profile, and choosing which topics are pinned
//   - most badges: their unlock rules need exam prep and quiz results first
//   - profile slots, if a few students share one machine

#include "app.h"
#include "theme.h"
#include "ui.h"
#include <algorithm>
#include <cstdlib>

const float SIDE_W = 280;
const float MAIN_X = MARGIN + SIDE_W + 24;
const float MAIN_W = SCREEN_W - MARGIN - MAIN_X;

// GitHub's dark heatmap greens: none, then four levels
const Color HEAT[5] = {
    { 33, 38, 45, 255 }, { 14, 68, 41, 255 }, { 0, 109, 50, 255 }, { 38, 166, 65, 255 }, { 57, 211, 83, 255 },
};

// One colour per track, in topics.txt order (like GitHub's language dots)
static Color track_color(const App &app, const std::string &track)
{
    const Color colors[] = { COL_BLUE, COL_PURPLE, COL_AMBER, COL_GREEN, COL_CORAL };
    std::vector<std::string> seen;
    for (const Topic &t : app.topics) {
        if (std::find(seen.begin(), seen.end(), t.track) == seen.end()) seen.push_back(t.track);
        if (t.track == track) break;
    }
    return colors[(seen.size() - 1) % 5];
}

static std::string topic_title(const App &app, const std::string &id)
{
    for (const Topic &t : app.topics) if (t.id == id) return t.title;
    return id;
}

// The computer's user name, until there's a name you can choose.
static std::string profile_name()
{
    const char *n = std::getenv("USERNAME");    // Windows
    if (!n || !*n) n = std::getenv("USER");     // Mac and Linux
    return (n && *n) ? n : "student";
}

static std::string initials(const std::string &name)
{
    std::string s;
    for (char c : name) if (isalpha((unsigned char)c) && s.size() < 2) s += (char)toupper((unsigned char)c);
    return s.empty() ? "?" : s;
}

// A solid colour part way from the background to c. (A see-through fill would
// show ui_panel's border box underneath it.)
static Color tint(Color c, float amount)
{
    return { (unsigned char)(COL_BG.r + (c.r - COL_BG.r) * amount), (unsigned char)(COL_BG.g + (c.g - COL_BG.g) * amount),
             (unsigned char)(COL_BG.b + (c.b - COL_BG.b) * amount), 255 };
}

// A label on the left, a value on the right.
static void stat_row(const std::string &label, const std::string &value, float x, float y, float w, Color color)
{
    ui_text(label, x, y, 14, COL_TEXT_SOFT);
    ui_text(value, x + w - ui_text_width(value, 14, FontKind::MonoBold), y + 1, 14, color, FontKind::MonoBold);
}

// ---------------------------------------------------------------------------
// Left: avatar, name, stats, badges
// ---------------------------------------------------------------------------

struct Badge {
    const char *icon;    // a few ASCII characters drawn in the circle
    const char *name;
    bool unlocked;
};

static void draw_sidebar(const App &app, float x, float y)
{
    const Progress &p = app.progress;
    std::string name = profile_name();
    float cx = x + SIDE_W / 2;

    // Avatar: a circle with initials, ringed in green
    DrawCircleV({ cx, y + 62 }, 62, COL_GREEN);
    DrawCircleV({ cx, y + 62 }, 59, COL_SURFACE_2);
    std::string ini = initials(name);
    ui_text(ini, cx - ui_text_width(ini, 40, FontKind::MonoBold) / 2, y + 38, 40, COL_GREEN, FontKind::MonoBold);
    y += 136;

    ui_text(name, cx - ui_text_width(name, 22, FontKind::UiBold) / 2, y, 22, COL_TEXT, FontKind::UiBold);
    y += 30;
    std::string handle = "@" + name + "  |  local profile";
    ui_text(handle, cx - ui_text_width(handle, 13, FontKind::Mono) / 2, y, 13, COL_MUTED, FontKind::Mono);
    y += 28;

    ui_button("Edit profile", { x, y, SIDE_W, 36 }, ButtonStyle::Disabled);
    y += 54;

    int streak = progress_streak(p, today_string());
    int answered = (int)p.answers.size();
    int correct = progress_correct_count(p);
    int maxPoints = (int)app.topics.size() * POINTS_MAX;
    std::string accuracy = answered ? std::to_string(correct * 100 / answered) + "%" : "-";

    stat_row("Current streak", std::to_string(streak) + (streak == 1 ? " day" : " days"), x, y, SIDE_W, COL_AMBER);
    y += 26;
    stat_row("Mastery points", std::to_string(progress_total_points(p)) + " / " + std::to_string(maxPoints), x, y, SIDE_W, COL_GREEN);
    y += 26;
    stat_row("Questions answered", std::to_string(answered), x, y, SIDE_W, COL_TEXT);
    y += 26;
    stat_row("Accuracy", accuracy, x, y, SIDE_W, COL_BLUE);
    y += 38;

    DrawLine((int)x, (int)y - 12, (int)(x + SIDE_W), (int)y - 12, COL_LINE_SOFT);
    ui_text("Achievements", x, y, 15, COL_TEXT, FontKind::UiBold);
    y += 30;

    bool leakHunter = topic_mastery(p, "membugs") == Mastery::Mastered;

    // Every "no []" problem in Exam prep solved
    int noBrackets = 0, noBracketsSolved = 0;
    for (const ProblemSet &set : app.practice)
        for (const Problem &prob : set.problems)
            if (forbids_token(prob, "[")) { noBrackets++; if (progress_is_solved(p, prob.id)) noBracketsSolved++; }
    bool noBracketsBadge = noBrackets > 0 && noBracketsSolved == noBrackets;
    std::vector<Badge> badges = {
        { "1st", "First answer", answered > 0 },
        { "5d", "5-day streak", streak >= 5 },
        { "A+", "10 right", correct >= 10 },
        { "[]", "No [] allowed", noBracketsBadge }, // every "no []" Exam prep problem solved
        { "!!", "Leak hunter", leakHunter },       // Memory bugs mastered
        { "SF", "Segfault survivor", false },      // still to decide
    };

    float colW = SIDE_W / 3;
    for (int i = 0; i < (int)badges.size(); i++) {
        const Badge &b = badges[i];
        float bx = x + (i % 3) * colW + colW / 2;
        float by = y + (i / 3) * 80 + 22;
        Color ring = b.unlocked ? COL_AMBER : COL_LINE;
        DrawCircleV({ bx, by }, 22, ring);
        DrawCircleV({ bx, by }, 20, b.unlocked ? tint(COL_AMBER, 0.15f) : COL_BG);
        ui_text(b.icon, bx - ui_text_width(b.icon, 14, FontKind::MonoBold) / 2, by - 9, 14,
                b.unlocked ? COL_AMBER : COL_LINE, FontKind::MonoBold);
        ui_text_centered_wrapped(b.name, bx, by + 28, colW - 6, 12, b.unlocked ? COL_TEXT_SOFT : COL_MUTED, 2);
    }
}

// ---------------------------------------------------------------------------
// Right: pinned topics, heatmap, recent activity
// ---------------------------------------------------------------------------

// The four topics with the most points. Ties go to topics with content, then
// map order, so a brand new profile shows the pointers lessons.
static std::vector<int> pinned_topics(const App &app)
{
    std::vector<int> order;
    for (int i = 0; i < (int)app.topics.size(); i++) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        int pa = topic_points(app.progress, app.topics[a].id), pb = topic_points(app.progress, app.topics[b].id);
        if (pa != pb) return pa > pb;
        bool ca = !app.topics[a].lessonFile.empty(), cb = !app.topics[b].lessonFile.empty();
        return ca && !cb;
    });
    if (order.size() > 4) order.resize(4);
    return order;
}

static void draw_pinned(App &app, float x, float y)
{
    ui_text("Pinned topics", x, y, 15, COL_TEXT, FontKind::UiBold);
    const char *note = "your strongest topics";
    ui_text(note, x + MAIN_W - ui_text_width(note, 13), y + 1, 13, COL_MUTED);
    y += 26;

    float cardW = (MAIN_W - 16) / 2, cardH = 76;
    std::vector<int> pinned = pinned_topics(app);
    for (int i = 0; i < (int)pinned.size(); i++) {
        const Topic &t = app.topics[pinned[i]];
        Rectangle r = { x + (i % 2) * (cardW + 16), y + (i / 2) * (cardH + 12), cardW, cardH };

        bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        ui_panel(r, hover ? COL_SURFACE_2 : COL_SURFACE, COL_LINE, 1);
        if (hover) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { app.selected = pinned[i]; open_map(app); }
        }

        Mastery m = topic_mastery(app.progress, t.id);
        int points = topic_points(app.progress, t.id);

        DrawCircleV({ r.x + 22, r.y + 24 }, 6, track_color(app, t.track));
        ui_text(t.title, r.x + 36, r.y + 14, 16, COL_TEXT, FontKind::UiBold);

        std::string level = mastery_name(m);
        Color lc = m == Mastery::NotStarted ? COL_MUTED : mastery_color(m);
        float lw = ui_text_width(level, 12, FontKind::Mono) + 14;
        ui_panel({ r.x + r.width - 16 - lw, r.y + 13, lw, 20 }, COL_BG, tint(lc, 0.7f), 1);
        ui_text(level, r.x + r.width - 16 - lw + 7, r.y + 16, 12, lc, FontKind::Mono);

        ui_text(t.track + (t.exam.empty() ? "" : "  |  " + t.exam), r.x + 36, r.y + 42, 13, COL_MUTED);
        std::string pts = std::to_string(points) + " / " + std::to_string(POINTS_MAX);
        ui_text(pts, r.x + r.width - 16 - ui_text_width(pts, 12, FontKind::Mono), r.y + 43, 12, COL_TEXT_SOFT, FontKind::Mono);

        Rectangle bar = { r.x + 16, r.y + r.height - 10, r.width - 32, 3 };
        DrawRectangleRec(bar, COL_LINE_SOFT);
        if (points > 0) DrawRectangleRec({ bar.x, bar.y, bar.width * points / POINTS_MAX, bar.height }, mastery_color(m));
    }
}

static int heat_level(int answers)
{
    if (answers <= 0) return 0;
    if (answers <= 2) return 1;
    if (answers <= 5) return 2;
    if (answers <= 9) return 3;
    return 4;
}

static void draw_heatmap(const App &app, float x, float y)
{
    const int WEEKS = 52;
    const float CELL = 13, GAP = 3, STEP = CELL + GAP;

    std::map<std::string, int> perDay = answers_per_day(app.progress);
    std::string today = today_string();

    // The grid ends with this week; its first column starts on a Monday.
    std::string start = date_add_days(today, -date_weekday(today) - (WEEKS - 1) * 7);
    int inYear = 0;
    for (const auto &entry : perDay)
        if (entry.first >= start && entry.first <= today) inYear += entry.second;

    ui_text("Activity", x, y, 15, COL_TEXT, FontKind::UiBold);
    std::string total = std::to_string(inYear) + (inYear == 1 ? " answer" : " answers") + " in the last year";
    ui_text(total, x + MAIN_W - ui_text_width(total, 13), y + 1, 13, COL_MUTED);
    y += 26;

    Rectangle panel = { x, y, MAIN_W, 172 };
    ui_panel(panel, COL_SURFACE, COL_LINE, 1);

    float gx = x + 48, gy = y + 34;
    const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const char *days[] = { "Mon", "", "Wed", "", "Fri", "", "" };
    for (int d = 0; d < 7; d++)
        if (*days[d]) ui_text(days[d], x + 14, gy + d * STEP, 11, COL_MUTED, FontKind::Mono);

    std::string hoverText;
    Vector2 mouse = GetMousePosition();
    std::string lastMonth;
    for (int w = 0; w < WEEKS; w++) {
        std::string monday = date_add_days(start, w * 7);
        std::string month = monday.substr(5, 2);
        if (month != lastMonth && w < WEEKS - 2) {
            ui_text(months[std::atoi(month.c_str()) - 1], gx + w * STEP, y + 12, 11, COL_MUTED, FontKind::Mono);
            lastMonth = month;
        }
        for (int d = 0; d < 7; d++) {
            std::string date = date_add_days(monday, d);
            if (date > today) break;
            auto it = perDay.find(date);
            int n = it == perDay.end() ? 0 : it->second;
            Rectangle cell = { gx + w * STEP, gy + d * STEP, CELL, CELL };
            DrawRectangleRounded(cell, 0.3f, 4, HEAT[heat_level(n)]);
            if (date == today) DrawRectangleLinesEx({ cell.x - 1, cell.y - 1, CELL + 2, CELL + 2 }, 1, COL_TEXT_SOFT);
            if (CheckCollisionPointRec(mouse, cell))
                hoverText = std::to_string(n) + (n == 1 ? " answer on " : " answers on ") + date;
        }
    }

    // Legend, and the hovered day on the left
    float ly = panel.y + panel.height - 24;
    float lx = panel.x + panel.width - 16 - 5 * STEP - ui_text_width("More", 11, FontKind::Mono);
    ui_text("Less", lx - 8 - ui_text_width("Less", 11, FontKind::Mono), ly, 11, COL_MUTED, FontKind::Mono);
    for (int i = 0; i < 5; i++) DrawRectangleRounded({ lx + i * STEP, ly + 1, CELL, CELL }, 0.3f, 4, HEAT[i]);
    ui_text("More", lx + 5 * STEP + 2, ly, 11, COL_MUTED, FontKind::Mono);
    ui_text(hoverText.empty() ? "Hover a square to see that day" : hoverText, gx, ly, 12,
            hoverText.empty() ? COL_MUTED : COL_TEXT, FontKind::Mono);
}

static void draw_recent(const App &app, float x, float y)
{
    ui_text("Recent activity", x, y, 15, COL_TEXT, FontKind::UiBold);
    y += 28;

    const std::vector<AnswerRecord> &answers = app.progress.answers;
    if (answers.empty()) {
        ui_text("Nothing yet. Answer a quiz question and it shows up here.", x, y, 14, COL_MUTED);
        return;
    }
    int shown = 0;
    for (int i = (int)answers.size() - 1; i >= 0 && shown < 4; i--, shown++) {
        const AnswerRecord &a = answers[i];
        Color c = a.correct ? COL_GREEN : COL_CORAL;
        DrawCircleV({ x + 6, y + 9 }, 5, c);
        std::string what = std::string(a.correct ? "Got a question right" : "Missed a question") + " in ";
        ui_text(what, x + 22, y, 14, COL_TEXT_SOFT);
        ui_text(topic_title(app, a.topic), x + 22 + ui_text_width(what, 14), y, 14, COL_TEXT, FontKind::UiBold);
        ui_text(a.date, x + MAIN_W - ui_text_width(a.date, 12, FontKind::Mono), y + 2, 12, COL_MUTED, FontKind::Mono);
        y += 26;
    }
}

void screen_profile(App &app)
{
    if (IsKeyPressed(KEY_ESCAPE)) { open_map(app); return; }
    if (IsKeyPressed(KEY_E)) { app.screen = Screen::ExamPrep; return; }

    ui_header("Profile - your progress, saved on this computer");
    if (ui_button("Back to the map", { SCREEN_W - MARGIN - 170, 12, 170, 40 }, ButtonStyle::Outline, COL_TEXT)) {
        open_map(app);
        return;
    }

    float top = HEADER_H + 22;
    draw_sidebar(app, MARGIN, top);
    DrawLine((int)(MAIN_X - 12), (int)top, (int)(MAIN_X - 12), SCREEN_H - FOOTER_H - 22, COL_LINE_SOFT);

    draw_pinned(app, MAIN_X, top);
    draw_heatmap(app, MAIN_X, top + 214);
    draw_recent(app, MAIN_X, top + 440);

    ui_footer_hints({ "click a pinned topic to open it on the map", "E exam prep", "ESC topic map" });
}
