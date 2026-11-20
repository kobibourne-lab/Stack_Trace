// screen_dailyq.cpp - the daily question: today's Exam prep problem, picked from
// all the sets, the same for everyone. Start opens it on the problem screen; solving
// it pays bytes and keeps a streak. The rules are in dailyq.cpp.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include <cmath>
#include <ctime>

static float copiedAt = -100;   // "Copied!" after the share button

void open_dailyq(App &app)
{
    if (app.screen != Screen::DailyQ) app.returnTo = app.screen;
    app.screen = Screen::DailyQ;
}

static Color difficulty_color(const std::string &d)
{
    if (d == "Easy") return COL_GREEN;
    if (d == "Hard") return COL_CORAL;
    return COL_AMBER;
}

// A small outlined tag; returns its width.
static float tag(const std::string &text, float x, float y, Color c)
{
    float w = ui_text_width(text, 13, FontKind::Mono) + 24;
    ui_panel({ x, y, w, 28 }, COL_SURFACE, c, 1);
    ui_text(text, x + 12, y + 6, 13, c, FontKind::Mono);
    return w;
}

// "5h 12m" until midnight, when the next question comes.
static std::string time_left()
{
    time_t now = time(nullptr);
    tm *lt = localtime(&now);
    int left = 24 * 3600 - (lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec);
    return std::to_string(left / 3600) + "h " + std::to_string(left % 3600 / 60) + "m";
}

static std::string clock_text(int seconds)
{
    return std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
}

// Streak, best streak, days solved and the time left: four numbers in a strip.
static void draw_stats(const App &app, Rectangle r)
{
    const DailyQStats &s = app.progress.dailyqStats;
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    int streak = dailyq_streak(app.progress, today_string());
    struct Cell { std::string big, small; Color c; } cells[4] = {
        { std::to_string(streak), "day streak", streak > 0 ? COL_CORAL : COL_MUTED },
        { std::to_string(s.best), "best streak", COL_AMBER },
        { std::to_string(s.solved), s.solved == 1 ? "day solved" : "days solved", COL_GREEN },
        { time_left(), "until the next one", COL_BLUE },
    };
    float cw = r.width / 4;
    for (int i = 0; i < 4; i++) {
        float x = r.x + i * cw;
        if (i > 0) DrawLine((int)x, (int)r.y + 10, (int)x, (int)(r.y + r.height - 10), COL_LINE_SOFT);
        ui_text(cells[i].big, x + 16, r.y + 8, 22, cells[i].c, FontKind::MonoBold);
        ui_text(cells[i].small, x + 16, r.y + 36, 13, COL_MUTED);
    }
}

void screen_dailyq(App &app)
{
    if (IsKeyPressed(KEY_ESCAPE)) { app.screen = app.returnTo; return; }
    std::string today = today_string();
    ui_header("Daily question  >  #" + std::to_string(dailyq_number(today)));

    std::vector<DailyRef> pool = dailyq_pool(app.practice);
    if (pool.empty()) {
        ui_text("No Exam prep problems loaded", MARGIN, HEADER_H + 40, 24, COL_CORAL, FontKind::UiBold);
        ui_text(app.practiceError, MARGIN, HEADER_H + 80, 16, COL_TEXT_SOFT, FontKind::Mono);
        ui_footer_hints({ "ESC back" });
        return;
    }
    DailyRef ref = pool[dailyq_index((int)pool.size(), today)];
    const ProblemSet &set = app.practice[ref.set];
    const Problem &p = set.problems[ref.problem];
    bool solved = dailyq_solved(app.progress, today);
    mal_set_base(app.mal, solved ? MalMood::Happy : MalMood::Idle);

    // ----- Left: the problem -----
    float x = MARGIN, w = 700, y = HEADER_H + 24;
    float tx = x;
    tx += tag(p.difficulty, tx, y, difficulty_color(p.difficulty)) + 8;
    tx += tag(set.exam.empty() ? set.title : set.title + "  |  " + set.exam, tx, y, set.exam == "Exam 2" ? COL_PURPLE : COL_BLUE) + 8;
    if (forbids_token(p, "[")) tx += tag("pointers only", tx, y, COL_AMBER) + 8;
    if (solved) tag("Solved today", tx, y, COL_GREEN);
    y += 44;
    y += ui_text_wrapped(p.title, x, y, w, 30, COL_TEXT, FontKind::UiBold) + 4;
    ui_text(p.function + "   |   " + p.group, x, y, 13, COL_MUTED, FontKind::Mono);
    y += 32;

    float protoH = ui_text_wrapped_height(p.prototype, w - 24, 14, FontKind::Mono) + 18;
    ui_panel({ x, y, w, protoH }, COL_BG, COL_LINE, 1);
    ui_text_wrapped(p.prototype, x + 12, y + 9, w - 24, 14, COL_TEXT_SOFT, FontKind::Mono);
    y += protoH + 16;

    ui_text("Task", x, y, 13, COL_MUTED, FontKind::UiBold);
    y += 22;
    if (!p.text.empty()) y += ui_text_wrapped(p.text[0], x, y, w, 16, COL_TEXT_SOFT) + 12;
    if (!p.rules.empty()) {
        ui_text("Rules", x, y, 13, COL_MUTED, FontKind::UiBold);
        y += 22;
        for (const std::string &rule : p.rules)
            y += ui_text_wrapped("-  " + rule, x, y, w, 15, COL_AMBER) + 4;
    }

    // ----- Right: start it, and how it pays -----
    float rx = x + w + 24, rw = SCREEN_W - MARGIN - rx, ry = HEADER_H + 24;
    int streak = dailyq_streak(app.progress, today);
    int reward = solved ? dailyq_reward(app.progress.dailyqStats.streak) : dailyq_reward(streak + 1);
    Rectangle card = { rx, ry, rw, 214 };
    ui_panel(card, solved ? Color{ 22, 38, 28, 255 } : COL_SURFACE, solved ? COL_GREEN : COL_LINE, 1.5f);
    if (solved) {
        ui_text("SOLVED TODAY", rx + 24, ry + 20, 13, COL_GREEN, FontKind::MonoBold);
        ui_text(clock_text(app.progress.dailyq.seconds), rx + 24, ry + 42, 40, COL_TEXT, FontKind::MonoBold);
        ui_text("+" + std::to_string(reward) + " bytes", rx + 24, ry + 96, 18, COL_PURPLE, FontKind::MonoBold);
        ui_text("Back for the next one in " + time_left(), rx + 24, ry + 124, 14, COL_TEXT_SOFT, FontKind::Mono);
    } else {
        ui_text("TODAY'S PAYOUT", rx + 24, ry + 20, 13, COL_MUTED, FontKind::MonoBold);
        ui_text("+" + std::to_string(reward) + " bytes", rx + 24, ry + 44, 34, COL_PURPLE, FontKind::MonoBold);
        ui_text_wrapped(std::to_string(DAILYQ_BYTES) + " for solving it, and " + std::to_string(DAILYQ_STREAK_BONUS) +
                        " more for each day already in your streak. As many tries as you like until midnight.",
                        rx + 24, ry + 92, rw - 48, 14, COL_TEXT_SOFT);
    }
    std::string label = solved ? "Open it again" : "Start";
    if (ui_button(label, { rx + 24, ry + card.height - 66, 180, 46 }, ButtonStyle::Primary, solved ? COL_GREEN : COL_AMBER) ||
        (IsKeyPressed(KEY_ENTER) && !solved)) {
        open_problem(app, ref.set, ref.problem, false);
        app.work.fromDaily = true;
        return;
    }
    bool copied = GetTime() - copiedAt < 1.5;
    if (ui_button(copied ? "Copied!" : "Copy result", { rx + 216, ry + card.height - 66, 150, 46 }, ButtonStyle::Outline, COL_TEXT)) {
        SetClipboardText(dailyq_share(app.progress, today).c_str());
        copiedAt = (float)GetTime();
    }

    mal_draw(app.mal, { rx + 50, ry + card.height + 130 }, 72);
    mal_say(solved ? "Nice one! A new problem arrives at midnight, from any lab or exam. Keep the streak going!"
                   : "A new problem every day, from all the labs and exams. Solve it before midnight to keep your streak.",
            rx + 110, ry + card.height + 34, rw - 110, { rx + 82, ry + card.height + 70 });

    draw_stats(app, { x, SCREEN_H - FOOTER_H - 74, w, 58 });
    if (solved) ui_footer_hints({ "ESC back", "M sound on/off" });
    else ui_footer_hints({ "ENTER start", "ESC back", "M sound on/off" });
}
