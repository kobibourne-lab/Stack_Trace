// screen_lesson.cpp - a lesson in three panes, Codecademy style:
// learn (text and the current step's explanation) | code | memory view.
// Left and right arrows step through the code one line at a time.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "memview.h"

static void next_step(App &app)
{
    if (app.step < (int)app.lesson.steps.size() - 1) {
        app.step++;
    } else {
        // Finished: on to the quiz if there is one, otherwise back to the map
        if (!app.topics[app.selected].quizFile.empty()) open_quiz(app);
        else open_map(app);
    }
}

static void prev_step(App &app)
{
    if (app.step > 0) app.step--;
}

static void draw_learn_pane(const App &app, Rectangle r)
{
    const Lesson &l = app.lesson;
    const Step &s = l.steps[app.step];
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    float x = r.x + 20, y = r.y + 18, w = r.width - 40;

    ui_text("LEARN", x, y, 12, COL_MUTED, FontKind::Mono);
    y += 24;
    y += ui_text_wrapped(l.title, x, y, w, 21, COL_TEXT, FontKind::UiBold) + 8;
    y += ui_text_wrapped(l.text, x, y, w, 15, COL_TEXT_SOFT) + 18;

    // Step counter with one segment per step
    std::string counter = "STEP " + std::to_string(app.step + 1) + " / " + std::to_string(l.steps.size());
    ui_text(counter, x, y, 12, COL_AMBER, FontKind::Mono);
    y += 22;
    int n = (int)l.steps.size();
    float segW = (w - (n - 1) * 4) / n;
    for (int i = 0; i < n; i++) {
        Color c = i < app.step ? COL_GREEN : (i == app.step ? COL_AMBER : COL_LINE);
        DrawRectangleRounded({ x + i * (segW + 4), y, segW, 6 }, 1, 4, c);
    }
    y += 22;

    // What just happened, in an amber box sized to fit it
    float textH = ui_text_wrapped_height(s.explain, w - 28, 15);
    ui_panel({ x, y, w, textH + 28 }, COL_SURFACE_2, COL_AMBER, 1);
    ui_text_wrapped(s.explain, x + 14, y + 14, w - 28, 15, COL_TEXT);
}

void screen_lesson(App &app)
{
    // Keys
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_SPACE)) next_step(app);
    if (app.screen != Screen::Lesson) return;   // finishing the lesson moved on
    if (IsKeyPressed(KEY_LEFT)) prev_step(app);
    if (IsKeyPressed(KEY_ESCAPE)) { open_map(app); return; }
    if (IsKeyPressed(KEY_Q) && !app.topics[app.selected].quizFile.empty()) { open_quiz(app); return; }

    const Topic &t = app.topics[app.selected];
    ui_header(t.track + "  >  " + t.title);

    float top = HEADER_H + 20;
    float bottom = SCREEN_H - FOOTER_H - 16;
    float h = bottom - top;
    Rectangle learn = { MARGIN, top, 300, h };
    Rectangle code = { learn.x + learn.width + 16, top, 380, h };
    Rectangle memory = { code.x + code.width + 16, top, SCREEN_W - MARGIN - (code.x + code.width + 16), h };

    draw_learn_pane(app, learn);
    ui_code(app.lesson.code, code, app.lesson.steps[app.step].line, COL_AMBER);
    memview_draw(app.lesson.steps[app.step], memory);

    ui_footer_hints({ "<- back a line", "-> next line", "Q quiz", "ESC topic map" });

    // Buttons, bottom right
    bool last = app.step == (int)app.lesson.steps.size() - 1;
    float by = SCREEN_H - FOOTER_H + 8, bh = 40;
    float x = SCREEN_W - MARGIN;
    std::string nextLabel = !last ? "Next line" : (t.quizFile.empty() ? "Finish" : "Take the quiz");
    float nw = ui_text_width(nextLabel, 16, FontKind::UiBold) + 40;
    x -= nw;
    if (ui_button(nextLabel, { x, by, nw, bh }, ButtonStyle::Primary)) { next_step(app); return; }
    x -= 12 + 90;
    if (ui_button("Back", { x, by, 90, bh }, app.step == 0 ? ButtonStyle::Disabled : ButtonStyle::Outline, COL_TEXT))
        prev_step(app);
}
