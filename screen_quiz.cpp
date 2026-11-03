// screen_quiz.cpp - one question at a time, w3schools style, with lives.
// Choice questions: pick an option (click, or press its number).
// Line questions: click the right line of code (spot the bug).
// After the last question (or the last life) the results are shown.

#include "app.h"
#include "theme.h"
#include "ui.h"

// The topic answers count towards: the quiz file's TOPIC, or the selected topic.
static std::string quiz_topic(const App &app)
{
    return app.quiz.quiz.topic.empty() ? app.topics[app.selected].id : app.quiz.quiz.topic;
}

static void answer(App &app, int choice)
{
    bool correct = quiz_answer(app.quiz, choice);
    record_answer(app, quiz_topic(app), correct);
}

// An option button that turns green or coral once the question is answered.
static bool option_button(const std::string &label, int number, Rectangle r, const QuizSession &s, const Question &q)
{
    Color border = COL_LINE, fill = COL_BG, text = COL_TEXT;
    bool clickable = !s.answered;
    bool hover = clickable && CheckCollisionPointRec(GetMousePosition(), r);

    if (s.answered && number == q.answer) { border = COL_GREEN; text = COL_GREEN; fill = COL_SURFACE_2; }
    else if (s.answered && number == s.chosen) { border = COL_CORAL; text = COL_CORAL; }
    else if (s.answered) text = COL_MUTED;
    else if (hover) { fill = COL_SURFACE_2; border = COL_TEXT_SOFT; ui_want_hand(); }

    ui_panel(r, fill, border, (s.answered && (number == q.answer || number == s.chosen)) ? 2.0f : 1.0f);
    ui_text(std::to_string(number), r.x + 16, r.y + 15, 15, COL_MUTED, FontKind::Mono);
    ui_text(label, r.x + 44, r.y + 14, 17, text, FontKind::Mono);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void draw_status(const App &app)
{
    const QuizSession &s = app.quiz;
    float x = SCREEN_W - MARGIN - 3 * 22;
    ui_hearts(x, 25, s.lives, START_LIVES);

    std::string score = "Score " + std::to_string(s.score);
    x -= ui_text_width(score, 14, FontKind::Mono) + 24;
    ui_text(score, x, 23, 14, COL_GREEN, FontKind::Mono);

    int total = (int)s.quiz.questions.size();
    int shown = s.index < total ? s.index + 1 : total;
    std::string count = "Question " + std::to_string(shown) + " / " + std::to_string(total);
    x -= ui_text_width(count, 14, FontKind::Mono) + 24;
    ui_text(count, x, 23, 14, COL_TEXT_SOFT, FontKind::Mono);
}

static void draw_question(App &app, const Question &q)
{
    QuizSession &s = app.quiz;
    float top = HEADER_H + 24;

    // ----- Left: the tag, the prompt and the code -----
    float x = MARGIN, w = 640, y = top;
    float tagW = ui_text_width(q.mode, 13, FontKind::Mono) + 24;
    ui_panel({ x, y, tagW, 28 }, COL_TEXT, COL_TEXT, 0);
    ui_text(q.mode, x + 12, y + 6, 13, COL_BG, FontKind::Mono);
    y += 44;
    y += ui_text_wrapped(q.prompt, x, y, w, 22, COL_TEXT, FontKind::UiBold) + 14;

    if (!q.code.empty()) {
        float codeH = q.code.size() * 16 * 1.55f + 20;
        bool clickLines = (q.kind == QuestionKind::Line) && !s.answered;
        int highlight = 0;
        Color hc = COL_GREEN;
        if (s.answered && q.kind == QuestionKind::Line) {
            highlight = s.lastCorrect ? q.answer : s.chosen;
            hc = s.lastCorrect ? COL_GREEN : COL_CORAL;
        }
        int clicked = ui_code(q.code, { x, y, w, codeH }, highlight, hc, clickLines);
        if (clicked > 0) answer(app, clicked);
        y += codeH + 10;
        if (q.kind == QuestionKind::Line && !s.answered)
            ui_text("Click a line to answer", x, y, 13, COL_MUTED);
    }

    // ----- Right: the options, then the feedback -----
    float rx = x + w + 24, rw = SCREEN_W - MARGIN - rx, ry = top;
    if (q.kind == QuestionKind::Choice) {
        for (int i = 0; i < (int)q.options.size(); i++) {
            int number = i + 1;
            if (option_button(q.options[i], number, { rx, ry, rw, 50 }, s, q)) answer(app, number);
            if (!s.answered && IsKeyPressed(KEY_ONE + i) && i < 9) answer(app, number);
            ry += 60;
        }
        ry += 10;
    }

    if (!s.answered) return;

    // Feedback
    std::string verdict = s.lastCorrect ? "Correct" : "Not quite";
    if (!s.lastCorrect && q.kind == QuestionKind::Line)
        verdict += ": the answer is line " + std::to_string(q.answer);
    Color vc = s.lastCorrect ? COL_GREEN : COL_CORAL;
    float textH = ui_text_wrapped_height(q.explain, rw - 40, 15);
    Rectangle box = { rx, ry, rw, textH + 140 };
    ui_panel(box, COL_SURFACE, vc, 1);
    ui_text(verdict, rx + 20, ry + 18, 20, vc, FontKind::UiBold);
    ui_text_wrapped(q.explain, rx + 20, ry + 54, rw - 40, 15, COL_TEXT_SOFT);

    bool last = quiz_is_last(s);
    std::string label = last ? "See results" : "Next question";
    float bw = ui_text_width(label, 16, FontKind::UiBold) + 40;
    if (ui_button(label, { rx + rw - 20 - bw, box.y + box.height - 60, bw, 42 }, ButtonStyle::Primary)
        || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_RIGHT))
        quiz_next(s);
}

static void draw_results(App &app)
{
    const QuizSession &s = app.quiz;
    std::string topic = quiz_topic(app);
    int total = (int)s.quiz.questions.size();
    float x = MARGIN, y = HEADER_H + 40;

    std::string big = std::to_string(s.score);
    ui_text(big, x, y, 96, COL_GREEN, FontKind::MonoBold);
    float bx = x + ui_text_width(big, 96, FontKind::MonoBold);
    ui_text(" / " + std::to_string(total), bx, y + 44, 44, COL_MUTED, FontKind::MonoBold);

    std::string headline = s.lives <= 0 ? "Out of lives" : (s.score == total ? "Full marks" : "Quiz complete");
    ui_text(headline, x + 280, y + 18, 28, COL_TEXT, FontKind::UiBold);
    Mastery m = topic_mastery(app.progress, topic);
    std::string now = app.topics[app.selected].title + " is now " + mastery_name(m) + "  ("
                      + std::to_string(topic_points(app.progress, topic)) + " / " + std::to_string(POINTS_MAX) + " pts)";
    ui_text(now, x + 280, y + 60, 16, m == Mastery::NotStarted ? COL_MUTED : mastery_color(m));
    y += 150;

    if (!s.missed.empty()) {
        ui_text("TO REVISE", x, y, 13, COL_AMBER, FontKind::Mono);
        y += 26;
        for (int i : s.missed) {
            const Question &q = s.quiz.questions[i];
            ui_text("Q" + std::to_string(i + 1) + "  " + q.mode, x, y, 15, COL_TEXT, FontKind::UiBold);
            ui_text_wrapped(q.prompt, x + 220, y, 700, 15, COL_TEXT_SOFT);
            y += 30;
        }
        y += 10;
    }

    y += 10;
    if (ui_button("Try again", { x, y, 140, 46 }, ButtonStyle::Primary) || IsKeyPressed(KEY_R)) {
        Quiz again = s.quiz;
        quiz_start(app.quiz, again);
        return;
    }
    if (ui_button("Back to the map", { x + 156, y, 180, 46 }, ButtonStyle::Outline, COL_TEXT) || IsKeyPressed(KEY_ENTER))
        open_map(app);
}

void screen_quiz(App &app)
{
    if (IsKeyPressed(KEY_ESCAPE)) { open_map(app); return; }

    std::string title = app.quiz.quiz.title.empty() ? app.topics[app.selected].title : app.quiz.quiz.title;
    ui_header("Quiz  >  " + title);
    draw_status(app);

    const Question *q = quiz_current(app.quiz);
    if (q != nullptr) {
        Question copy = *q;   // answering can change the session, so draw from a copy
        draw_question(app, copy);
        ui_footer_hints({ "1-4 choose", "ENTER next", "ESC topic map" });
    } else {
        draw_results(app);
        ui_footer_hints({ "R try again", "ENTER topic map", "ESC topic map" });
    }
}
