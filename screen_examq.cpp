// screen_examq.cpp - the exam question of the day: one question picked from all
// the material, the same for everyone on that date, and ONE go at it. The topic
// stays hidden until you've answered. The rules are in examq.cpp.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include <cmath>
#include <ctime>

static float answeredAt = -100;   // GetTime() when it was answered here (for the pop and the shake)
static float copiedAt = -100;     // "Copied!" after the share button

void open_examq(App &app)
{
    if (app.screen != Screen::ExamQ) app.returnTo = app.screen;
    app.screen = Screen::ExamQ;
}

// "5h 12m" until midnight, when the next question comes.
static std::string time_left()
{
    time_t now = time(nullptr);
    tm *lt = localtime(&now);
    int left = 24 * 3600 - (lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec);
    return std::to_string(left / 3600) + "h " + std::to_string(left % 3600 / 60) + "m";
}

static int topic_index(const App &app, const std::string &id)
{
    for (int i = 0; i < (int)app.topics.size(); i++) if (app.topics[i].id == id) return i;
    return -1;
}

// Answer, then react: the same bursts and sounds as a quiz answer.
static void answer(App &app, const ExamQ &e, int choice, Vector2 at)
{
    std::string today = today_string();
    bool correct = false;
    int bytes = 0;
    std::string why;
    if (!examq_answer(app.progress, today, e, choice, correct, bytes, why)) return;
    record_answer(app, e.topic, correct);    // topic mastery, XP, the streak and the answer quests; saves
    quest_event(app, "examq");               // the "Answer today's exam question" quest; saves
    answeredAt = (float)GetTime();

    if (correct) {
        int streak = app.progress.examqStats.streak;
        fx_burst(at, 34, COL_GREEN, COL_AMBER);
        fx_float_text("+" + std::to_string(XP_CORRECT) + " XP", { at.x, at.y - 34 }, 26, COL_AMBER);
        fx_float_text("+" + std::to_string(bytes) + " bytes", { at.x, at.y - 70 }, 22, COL_PURPLE);
        if (streak >= 2) fx_float_text(std::to_string(streak) + " day streak", { at.x, at.y - 104 }, 20, COL_CORAL);
        sfx_play(streak >= 3 ? Sfx::Combo : Sfx::Correct);
        mal_react(app.mal, streak >= 3 ? MalMood::Cheer : MalMood::Happy, 2.2f);
    } else {
        fx_shake(0.5f);
        fx_float_text("+" + std::to_string(bytes) + " bytes for trying", { at.x, at.y - 40 }, 18, COL_MUTED);
        sfx_play(Sfx::Wrong);
        mal_react(app.mal, MalMood::Sad, 2.0f);
    }
}

// The three little numbers under the question: streak, answered, right.
static void draw_stats(const App &app, Rectangle r)
{
    const ExamQStats &s = app.progress.examqStats;
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    int streak = examq_streak(app.progress, today_string());
    int pct = s.played > 0 ? (int)lroundf(100.0f * s.correct / s.played) : 0;
    struct Cell { std::string big, small; Color c; } cells[4] = {
        { std::to_string(streak), "day streak", streak > 0 ? COL_CORAL : COL_MUTED },
        { std::to_string(s.best), "best streak", COL_AMBER },
        { s.played > 0 ? std::to_string(pct) + "%" : "-", std::to_string(s.correct) + " of " + std::to_string(s.played) + " right", COL_GREEN },
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

static void draw_question(App &app, const ExamQ &e, bool answered, int chosen, bool correct)
{
    const Question &q = e.q;
    std::string today = today_string();
    float top = HEADER_H + 24;
    float age = (float)(GetTime() - answeredAt);
    mal_set_base(app.mal, MalMood::Idle);

    // ----- Left: the tags, the prompt and the code -----
    float x = MARGIN, w = 640, y = top;
    float tagW = ui_text_width(q.mode, 13, FontKind::Mono) + 24;
    ui_panel({ x, y, tagW, 28 }, COL_TEXT, COL_TEXT, 0);
    ui_text(q.mode, x + 12, y + 6, 13, COL_BG, FontKind::Mono);
    // Which topic it came from is the answer's business: it only shows once you've answered
    std::string from = answered ? e.topicTitle + (e.exam.empty() ? "" : "  |  " + e.exam) : "Any topic, any lab or lesson";
    float fromW = ui_text_width(from, 13, FontKind::Mono) + 24;
    ui_panel({ x + tagW + 10, y, fromW, 28 }, COL_SURFACE, answered ? COL_PURPLE : COL_LINE, 1);
    ui_text(from, x + tagW + 22, y + 6, 13, answered ? COL_PURPLE : COL_MUTED, FontKind::Mono);
    y += 44;
    y += ui_text_wrapped(q.prompt, x, y, w, 22, COL_TEXT, FontKind::UiBold) + 14;

    if (!q.code.empty()) {
        const float lineH = 16 * 1.55f;
        float codeH = q.code.size() * lineH + 20;
        bool clickLines = (q.kind == QuestionKind::Line) && !answered;
        int highlight = 0;
        Color hc = COL_GREEN;
        if (answered && q.kind == QuestionKind::Line) {
            highlight = correct ? q.answer : chosen;
            hc = correct ? COL_GREEN : COL_CORAL;
        }
        float codeY = y;
        int clicked = ui_code(q.code, { x, y, w, codeH }, highlight, hc, clickLines);
        if (clicked > 0) answer(app, e, clicked, { x + w * 0.45f, codeY + 10 + (clicked - 0.5f) * lineH });
        y += codeH + 10;
        if (q.kind == QuestionKind::Line && !answered) ui_text("Click a line to answer", x, y, 13, COL_MUTED);
    }
    draw_stats(app, { x, SCREEN_H - FOOTER_H - 74, w, 58 });

    // ----- Right: the options, then Mal and the feedback -----
    float rx = x + w + 24, rw = SCREEN_W - MARGIN - rx, ry = top;
    if (q.kind == QuestionKind::Choice) {
        for (int i = 0; i < (int)q.options.size(); i++) {
            int number = i + 1;
            Rectangle r = { rx, ry, rw, 54 };
            Vector2 centre = { r.x + r.width * 0.5f, r.y + 25 };
            if (quiz_option_button(q.options[i], number, r, answered, chosen, q.answer, age)) answer(app, e, number, centre);
            if (!answered && i < 9 && IsKeyPressed(KEY_ONE + i)) answer(app, e, number, centre);
            ry += 64;
        }
        ry += 6;
    }

    if (!answered) {
        mal_draw(app.mal, { rx + 50, ry + 110 }, 72);
        mal_say("One go, like the real exam: no lives and no second try. It could be from any topic. Take your time!",
                rx + 110, ry + 14, rw - 110, { rx + 82, ry + 52 });
        return;
    }

    // Feedback: a banner that slides up
    float slide = ease_out_cubic(age / 0.22f);
    std::string verdict = correct ? "Correct!" : "Not quite";
    if (!correct && q.kind == QuestionKind::Line) verdict += ": it's line " + std::to_string(q.answer);
    Color vc = correct ? COL_GREEN : COL_CORAL;
    float textX = rx + 120, textW = rw - 140;
    float textH = ui_text_wrapped_height(q.explain, textW, 15);
    float boxH = fmaxf(textH + 190, 240);
    Rectangle box = { rx, ry + (1 - slide) * 40, rw, boxH };
    ui_panel(box, correct ? Color{ 22, 38, 28, 255 } : Color{ 40, 24, 26, 255 }, vc, 1.5f);
    mal_draw(app.mal, { box.x + 62, box.y + 120 }, 80);
    ui_text(verdict, textX, box.y + 18, 21, vc, FontKind::UiBold);
    ui_text_wrapped(q.explain, textX, box.y + 54, textW, 15, COL_TEXT_SOFT);
    int bytes = examq_reward(correct, app.progress.examqStats.streak);
    ui_text("+" + std::to_string(bytes) + " bytes" + (correct ? "" : " for having a go") +
            "   |   come back in " + time_left() + " for the next one",
            box.x + 20, box.y + box.height - 106, 14, COL_PURPLE, FontKind::Mono);

    float by = box.y + box.height - 62, bx = box.x + 20;
    int ti = topic_index(app, e.topic);
    if (ti >= 0 && ui_button("Revise this topic", { bx, by, 190, 46 }, ButtonStyle::Primary, vc)) {
        app.selected = ti;
        app.inCampaign = false;
        open_lesson(app);
        return;
    }
    bool copied = GetTime() - copiedAt < 1.5;
    if (ui_button(copied ? "Copied!" : "Copy result", { bx + 202, by, 150, 46 }, ButtonStyle::Outline, COL_TEXT)) {
        SetClipboardText(examq_share(app.progress, today).c_str());
        copiedAt = (float)GetTime();
    }
    if (ui_button("Back", { bx + 364, by, 100, 46 }, ButtonStyle::Outline, COL_MUTED)) { app.screen = app.returnTo; return; }
}

void screen_examq(App &app)
{
    if (IsKeyPressed(KEY_ESCAPE)) { app.screen = app.returnTo; return; }
    std::string today = today_string();
    ui_header("Exam question of the day  >  #" + std::to_string(examq_number(today)));

    if (app.examPool.empty()) {
        ui_text("No quiz questions loaded", MARGIN, HEADER_H + 40, 24, COL_CORAL, FontKind::UiBold);
        ui_text(app.examPoolError, MARGIN, HEADER_H + 80, 16, COL_TEXT_SOFT, FontKind::Mono);
        ui_footer_hints({ "ESC back" });
        return;
    }
    ExamQ e = app.examPool[examq_index((int)app.examPool.size(), today)];   // a copy: answering changes progress
    bool answered = examq_answered(app.progress, today);
    int chosen = examq_choice(app.progress, today);
    bool correct = examq_correct(app.progress, today);
    draw_question(app, e, answered, chosen, correct);
    if (answered) ui_footer_hints({ "ESC back", "M sound on/off" });
    else ui_footer_hints({ "1-4 choose", "ESC back (it'll wait for you until midnight)", "M sound on/off" });
}
