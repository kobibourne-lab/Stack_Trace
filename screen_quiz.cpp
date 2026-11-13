// screen_quiz.cpp - one question at a time, w3schools style, with lives.
// Choice questions: pick an option (click, or press its number).
// Line questions: click the right line of code (spot the bug).
// After the last question (or the last life) the results are shown.
//
// Every answer gets a reaction, Duolingo style: a right one pops, bursts and
// floats "+10 XP"; a wrong one shakes, cracks a heart and buzzes. Mal reacts
// too, and runs of right answers build a combo.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include <cmath>

// Animation timers for this screen. They only change how things look.
static float answeredAt = -100;   // GetTime() when the current question was answered
static float shownProgress = 0;   // the progress bar's fill, easing towards the real value
static int comboShown = 0;        // the combo the badge last showed, to pop it when it grows
static float comboPopAt = -100;

// The topic answers count towards: the quiz file's TOPIC, or the selected topic.
static std::string quiz_topic(const App &app)
{
    return app.quiz.quiz.topic.empty() ? app.topics[app.selected].id : app.quiz.quiz.topic;
}

static float since_answer() { return (float)(GetTime() - answeredAt); }

// Where the hearts are drawn (draw_status), so a lost one can break in place.
static Vector2 heart_position(int index)
{
    return { (float)(SCREEN_W - MARGIN - 3 * 22 + index * 22 + 8), 33.0f };
}

// Answer, then react: `at` is where the answer was clicked, for the effects.
static void answer(App &app, int choice, Vector2 at)
{
    QuizSession &s = app.quiz;
    bool correct = quiz_answer(s, choice);
    record_answer(app, quiz_topic(app), correct);
    if (correct) quest_best(app, "combo", s.combo);
    answeredAt = (float)GetTime();

    if (correct) {
        fx_burst(at, 26, COL_GREEN, COL_AMBER);
        fx_float_text("+" + std::to_string(XP_CORRECT) + " XP", { at.x, at.y - 34 }, 26, COL_AMBER);
        if (s.combo >= 3) {
            sfx_play(Sfx::Combo);
            const char *word = s.combo >= 5 ? "UNSTOPPABLE" : "ON FIRE";
            fx_float_text(std::string(word) + " x" + std::to_string(s.combo), { at.x, at.y - 74 }, 22, COL_CORAL);
            mal_react(app.mal, MalMood::Cheer, 2.2f);
        } else {
            sfx_play(Sfx::Correct);
            mal_react(app.mal, MalMood::Happy, 1.6f);
        }
    } else {
        fx_shake(0.5f);
        Vector2 heart = heart_position(s.lives);   // the heart that was just lost
        fx_burst(heart, 14, COL_CORAL, COL_BG);
        fx_float_text("-1", { heart.x, heart.y + 18 }, 18, COL_CORAL);
        if (s.lives <= 0) {
            sfx_play(Sfx::Crumble);
            mal_react(app.mal, MalMood::Freed, 0);   // stays crumbled until you try again
        } else {
            sfx_play(Sfx::Wrong);
            mal_react(app.mal, MalMood::Sad, 1.8f);
        }
    }
}

// An option: a chunky button that pops green when it's the answer, and shakes
// when it was picked wrongly.
static bool option_button(const std::string &label, int number, Rectangle r, const QuizSession &s, const Question &q)
{
    Color border = COL_LINE, fill = COL_SURFACE, text = COL_TEXT, base = COL_LINE;
    bool clickable = !s.answered;
    bool hover = clickable && CheckCollisionPointRec(GetMousePosition(), r);
    bool held = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    float age = since_answer();

    bool isAnswer = s.answered && number == q.answer;
    bool isWrongPick = s.answered && number == s.chosen && !isAnswer;
    if (isAnswer) { border = COL_GREEN; text = COL_GREEN; fill = { 29, 58, 38, 255 }; base = { 47, 107, 58, 255 }; }
    else if (isWrongPick) { border = COL_CORAL; text = COL_CORAL; fill = { 58, 30, 30, 255 }; base = { 110, 48, 45, 255 }; }
    else if (s.answered) text = COL_MUTED;
    else if (hover) { fill = COL_SURFACE_2; border = COL_TEXT_SOFT; base = COL_TEXT_SOFT; ui_want_hand(); }

    // Pop: the right answer swells and settles. Shake: the wrong pick wobbles.
    if (isAnswer && age < 0.45f) {
        float grow = sinf(fminf(1.0f, age / 0.45f) * PI) * 6;
        r = { r.x - grow, r.y - grow / 2, r.width + 2 * grow, r.height + grow };
    }
    if (isWrongPick && age < 0.45f) r.x += sinf(age * 55) * 9 * (1 - age / 0.45f);

    const float depth = 4;
    ui_panel({ r.x, r.y + depth, r.width, r.height - depth }, base, base, 0);
    float sink = held ? depth - 1 : 0;
    Rectangle face = { r.x, r.y + sink, r.width, r.height - depth };
    ui_panel(face, fill, border, (isAnswer || isWrongPick) ? 2.0f : 1.0f);

    // The number in a little key cap, then the option
    Rectangle key = { face.x + 12, face.y + (face.height - 26) / 2, 26, 26 };
    ui_panel(key, COL_BG, isAnswer ? COL_GREEN : isWrongPick ? COL_CORAL : COL_LINE, 1);
    std::string n = std::to_string(number);
    ui_text(n, key.x + (key.width - ui_text_width(n, 14, FontKind::MonoBold)) / 2, key.y + 4, 14,
            isAnswer ? COL_GREEN : isWrongPick ? COL_CORAL : COL_MUTED, FontKind::MonoBold);
    ui_text(label, face.x + 52, face.y + (face.height - 21) / 2, 17, text, FontKind::Mono);
    if (isAnswer) {   // a tick on the right answer
        float cx = face.x + face.width - 28, cy = face.y + face.height / 2;
        DrawLineEx({ cx - 8, cy }, { cx - 2, cy + 6 }, 3.5f, COL_GREEN);
        DrawLineEx({ cx - 2, cy + 6 }, { cx + 9, cy - 7 }, 3.5f, COL_GREEN);
    }

    bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    return clicked;
}

static void draw_status(App &app)
{
    const QuizSession &s = app.quiz;
    float x = SCREEN_W - MARGIN - 3 * 22;
    ui_hearts(x, 25, s.lives, START_LIVES);

    // The progress bar: fills as questions are answered, easing rather than jumping
    int total = (int)s.quiz.questions.size();
    float target = total > 0 ? fminf(1.0f, (float)(s.index + (s.answered ? 1 : 0)) / total) : 0;
    shownProgress += (target - shownProgress) * fminf(1.0f, GetFrameTime() * 8);
    float bw = 220;
    x -= bw + 24;
    Rectangle bar = { x, 26, bw, 12 };
    DrawRectangleRounded(bar, 1, 8, COL_LINE_SOFT);
    if (shownProgress > 0.01f) {
        Rectangle fill = { bar.x, bar.y, bar.width * shownProgress, bar.height };
        DrawRectangleRounded(fill, 1, 8, COL_GREEN);
        DrawRectangleRounded({ fill.x + 4, fill.y + 2, fmaxf(0.0f, fill.width - 8), 3 }, 1, 6, Fade(WHITE, 0.35f));   // shine
    }

    // The combo badge, from 2 in a row, popping each time it grows
    if (s.combo != comboShown) { if (s.combo > comboShown) comboPopAt = (float)GetTime(); comboShown = s.combo; }
    if (s.combo >= 2) {
        float pop = 1 + 0.35f * sinf(fminf(1.0f, (float)(GetTime() - comboPopAt) / 0.3f) * PI);
        std::string c = "x" + std::to_string(s.combo) + " combo";
        float size = 14 * pop;
        float cw = ui_text_width(c, size, FontKind::MonoBold) + 20;
        Color cc = s.combo >= 3 ? COL_CORAL : COL_AMBER;
        float cx = x - 16 - cw;
        ui_panel({ cx, 32 - 14 * pop + 3, cw, 28 * pop }, cc, cc, 0);
        ui_text(c, cx + 10, 32 - size * 0.62f + 3, size, COL_BG, FontKind::MonoBold);
    }
}

static void draw_question(App &app, const Question &q)
{
    QuizSession &s = app.quiz;
    float top = HEADER_H + 24;
    mal_set_base(app.mal, s.lives == 1 ? MalMood::Worried : MalMood::Idle);

    // ----- Left: the tag, the prompt and the code -----
    float x = MARGIN, w = 640, y = top;
    float tagW = ui_text_width(q.mode, 13, FontKind::Mono) + 24;
    ui_panel({ x, y, tagW, 28 }, COL_TEXT, COL_TEXT, 0);
    ui_text(q.mode, x + 12, y + 6, 13, COL_BG, FontKind::Mono);
    y += 44;
    y += ui_text_wrapped(q.prompt, x, y, w, 22, COL_TEXT, FontKind::UiBold) + 14;

    if (!q.code.empty()) {
        const float lineH = 16 * 1.55f;
        float codeH = q.code.size() * lineH + 20;
        bool clickLines = (q.kind == QuestionKind::Line) && !s.answered;
        int highlight = 0;
        Color hc = COL_GREEN;
        if (s.answered && q.kind == QuestionKind::Line) {
            highlight = s.lastCorrect ? q.answer : s.chosen;
            hc = s.lastCorrect ? COL_GREEN : COL_CORAL;
        }
        float codeY = y;
        int clicked = ui_code(q.code, { x, y, w, codeH }, highlight, hc, clickLines);
        if (clicked > 0) answer(app, clicked, { x + w * 0.45f, codeY + 10 + (clicked - 0.5f) * lineH });
        y += codeH + 10;
        if (q.kind == QuestionKind::Line && !s.answered)
            ui_text("Click a line to answer", x, y, 13, COL_MUTED);
    }

    // ----- Right: the options, then Mal and the feedback -----
    float rx = x + w + 24, rw = SCREEN_W - MARGIN - rx, ry = top;
    if (q.kind == QuestionKind::Choice) {
        for (int i = 0; i < (int)q.options.size(); i++) {
            int number = i + 1;
            Rectangle r = { rx, ry, rw, 54 };
            Vector2 centre = { r.x + r.width * 0.5f, r.y + 25 };
            if (option_button(q.options[i], number, r, s, q)) answer(app, number, centre);
            if (!s.answered && IsKeyPressed(KEY_ONE + i) && i < 9) answer(app, number, centre);
            ry += 64;
        }
        ry += 6;
    }

    if (!s.answered) {
        // Mal waits beside a hint
        mal_draw(app.mal, { rx + 50, ry + 110 }, 72);
        const char *hint = q.kind == QuestionKind::Line ? "Click the line you think is wrong."
                                                        : "Take your time. Click an option, or press its number.";
        if (s.lives == 1) hint = "Careful, that's our last heart. One more wrong and I get free()'d!";
        mal_say(hint, rx + 110, ry + 14, rw - 110, { rx + 82, ry + 52 });
        return;
    }

    // Feedback: a banner that slides up, with Mal reacting on its left
    float slide = ease_out_cubic(since_answer() / 0.22f);
    std::string verdict = s.lastCorrect ? (s.combo >= 3 ? "Brilliant!" : "Correct!") : "Not quite";
    if (!s.lastCorrect && q.kind == QuestionKind::Line)
        verdict += ": it's line " + std::to_string(q.answer);
    Color vc = s.lastCorrect ? COL_GREEN : COL_CORAL;
    float textX = rx + 120, textW = rw - 140;
    float textH = ui_text_wrapped_height(q.explain, textW, 15);
    float boxH = fmaxf(textH + 140, 190);
    Rectangle box = { rx, ry + (1 - slide) * 40, rw, boxH };
    Color tint = s.lastCorrect ? Color{ 22, 38, 28, 255 } : Color{ 40, 24, 26, 255 };
    ui_panel(box, tint, vc, 1.5f);
    mal_draw(app.mal, { box.x + 62, box.y + boxH - 22 }, 80);
    ui_text(verdict, textX, box.y + 18, 21, vc, FontKind::UiBold);
    ui_text_wrapped(q.explain, textX, box.y + 54, textW, 15, COL_TEXT_SOFT);

    bool last = quiz_is_last(s);
    std::string label = last ? "See results" : "Continue";
    float bw = ui_text_width(label, 16, FontKind::UiBold) + 48;
    if (ui_button(label, { box.x + box.width - 20 - bw, box.y + box.height - 64, bw, 48 }, ButtonStyle::Primary, vc)
        || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_RIGHT)) {
        quiz_next(s);
        if (quiz_finished(s) && s.lives > 0) {
            // Passed: mastery from the lives left (this clears the topic in the campaign)
            progress_finish_quiz(app.progress, quiz_topic(app), s.lives, START_LIVES);
            quest_event(app, "quiz");   // saves
        }
        if (quiz_finished(s) && s.lives > 0) {   // results: celebrate a good run
            int total = (int)s.quiz.questions.size();
            if (s.score == total) {
                mal_react(app.mal, MalMood::Cheer, 0);
                sfx_play(Sfx::LevelUp);
                fx_burst({ 200, 200 }, 50, COL_GREEN, COL_AMBER);
            } else {
                mal_react(app.mal, MalMood::Happy, 1.6f);
            }
        }
    }
}

static void draw_results(App &app)
{
    QuizSession &s = app.quiz;
    std::string topic = quiz_topic(app);
    int total = (int)s.quiz.questions.size();
    float x = MARGIN, y = HEADER_H + 40;
    bool crashed = s.lives <= 0, perfect = s.score == total;
    if (!crashed && !perfect) mal_set_base(app.mal, MalMood::Idle);

    std::string big = std::to_string(s.score);
    ui_text(big, x, y, 96, crashed ? COL_CORAL : COL_GREEN, FontKind::MonoBold);
    float bx = x + ui_text_width(big, 96, FontKind::MonoBold);
    ui_text(" / " + std::to_string(total), bx, y + 44, 44, COL_MUTED, FontKind::MonoBold);

    std::string headline = crashed ? "Out of lives" : (perfect ? "Full marks" : "Quiz complete");
    ui_text(headline, x + 280, y + 6, 28, COL_TEXT, FontKind::UiBold);
    Mastery m = topic_mastery(app.progress, topic);
    std::string now = app.topics[app.selected].title + " is now " + mastery_name(m) + "  ("
                      + std::to_string(topic_points(app.progress, topic)) + " / " + std::to_string(POINTS_MAX) + " pts)";
    ui_text(now, x + 280, y + 46, 16, m == Mastery::NotStarted ? COL_MUTED : mastery_color(m));
    if (!crashed) {   // how the lives left turned into mastery
        std::string passed = "Passed with " + std::to_string(s.lives) + " of " + std::to_string(START_LIVES) + " lives: "
                           + std::to_string(quiz_mastery_points(s.lives, START_LIVES)) + " mastery"
                           + (s.lives == START_LIVES ? ". Perfect!" : ". Keep every life for 100.");
        ui_text(passed, x + 280, y + 106, 15, COL_TEXT_SOFT);
    } else {
        ui_text("Not passed: finish with at least one life left to clear this topic.", x + 280, y + 106, 15, COL_TEXT_SOFT);
    }

    // What this quiz earned, RPG style
    int wrong = (int)s.missed.size();
    int earned = s.score * XP_CORRECT + wrong * XP_WRONG;
    std::string xp = "+" + std::to_string(earned) + " XP";
    ui_text(xp, x + 280, y + 76, 18, COL_AMBER, FontKind::MonoBold);
    if (s.bestCombo >= 2)
        ui_text("best combo x" + std::to_string(s.bestCombo), x + 290 + ui_text_width(xp, 18, FontKind::MonoBold), y + 78, 16,
                COL_CORAL, FontKind::Mono);
    y += 160;

    ui_level_badge(x, y, progress_xp(app.progress));
    y += 60;

    if (!s.missed.empty()) {
        ui_text("TO REVISE", x, y, 13, COL_AMBER, FontKind::Mono);
        y += 26;
        for (int i : s.missed) {
            const Question &q = s.quiz.questions[i];
            ui_text("Q" + std::to_string(i + 1) + "  " + q.mode, x, y, 15, COL_TEXT, FontKind::UiBold);
            ui_text_wrapped(q.prompt, x + 220, y, 480, 15, COL_TEXT_SOFT);
            y += 30;
        }
        y += 10;
    }

    y += 10;
    if (ui_button("Try again", { x, y, 150, 50 }, ButtonStyle::Primary) || IsKeyPressed(KEY_R)) {
        Quiz again = s.quiz;
        quiz_start(app.quiz, again);
        mal_react(app.mal, MalMood::Happy, 1.2f);   // builds itself back up if it was freed
        return;
    }
    if (ui_button("Back to the map", { x + 166, y, 190, 50 }, ButtonStyle::Outline, COL_TEXT) || IsKeyPressed(KEY_ENTER)) {
        open_map(app);
        return;
    }

    // Mal, big, on the right, with something to say
    float mx = 1010, feet = SCREEN_H - FOOTER_H - 40;
    mal_draw(app.mal, { mx, feet }, 150);
    std::string line = crashed ? "free(mal);  ...that one crashed me. Press R and I'll malloc myself back."
                     : perfect ? "Full marks! Not a single leak. I'm framing this."
                               : "Not bad! Have another go at the ones under TO REVISE.";
    if (crashed) mal_say(line, mx - 170, feet - 150, 300, { mx, feet - 40 });   // pointing at the pile
    else mal_say(line, mx - 170, feet - 290, 300, { mx - 20, feet - 200 });
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
        ui_footer_hints({ "1-4 choose", "ENTER continue", "M sound on/off", "ESC topic map" });
    } else {
        draw_results(app);
        ui_footer_hints({ "R try again", "ENTER topic map", "ESC topic map" });
    }
}
