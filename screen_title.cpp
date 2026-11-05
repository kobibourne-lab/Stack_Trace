// screen_title.cpp - the start screen: the logo and name, a menu, and a small
// memory view on the right that loops through a pointer example on its own,
// to show what the app is about before you've pressed anything.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include <cmath>

// The example on the right, one line at a time
static const std::vector<std::string> DEMO_CODE = {
    "int x = 5;",
    "int *p = &x;",
    "*p = 7;",
    "printf(\"%d\", x);",
};
const double DEMO_STEP_SECONDS = 1.8;

// One variable in the demo's stack frame: a name, a type and a box for its value.
static Rectangle demo_variable(const std::string &name, const std::string &type, const std::string &value,
                               float x, float y, Color border, bool justChanged)
{
    ui_text(name, x, y + 12, 18, COL_TEXT, FontKind::MonoBold);
    ui_text(type, x, y + 36, 12, COL_MUTED, FontKind::Mono);
    Rectangle box = { x + 72, y, 120, 54 };
    ui_panel(box, justChanged ? COL_SURFACE_2 : COL_BG, border, justChanged ? 2 : 1);
    Color vc = justChanged ? COL_AMBER : COL_TEXT;
    ui_text(value, box.x + box.width / 2 - ui_text_width(value, 20, FontKind::MonoBold) / 2, box.y + 15, 20, vc,
            FontKind::MonoBold);
    return box;
}

static void draw_demo(Rectangle r)
{
    int step = (int)(GetTime() / DEMO_STEP_SECONDS) % (int)DEMO_CODE.size();   // 0 to 3

    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    float x = r.x + 24, y = r.y + 20;
    ui_text("WATCH IT RUN", x, y, 12, COL_MUTED, FontKind::MonoBold);
    std::string line = "line " + std::to_string(step + 1) + " of " + std::to_string(DEMO_CODE.size());
    ui_text(line, r.x + r.width - 24 - ui_text_width(line, 12, FontKind::Mono), y, 12, COL_MUTED, FontKind::Mono);
    y += 28;

    ui_code(DEMO_CODE, { x, y, r.width - 48, 132 }, step + 1, COL_AMBER, false, 15);
    y += 152;

    // The stack frame for main, with x and (from line 2) p
    Rectangle frame = { x, y, r.width - 48, 178 };
    ui_panel(frame, COL_BG, COL_PURPLE, 1);
    ui_text("main", frame.x + 14, frame.y + 10, 13, COL_PURPLE, FontKind::MonoBold);
    ui_text("stack frame", frame.x + 14 + ui_text_width("main ", 13, FontKind::MonoBold), frame.y + 10, 13, COL_MUTED,
            FontKind::Mono);

    Rectangle xBox = demo_variable("x", "int", step >= 2 ? "7" : "5", frame.x + 18, frame.y + 40, COL_LINE, step == 2);
    if (step >= 1) {
        Rectangle pBox = demo_variable("p", "int *", "&x", frame.x + 18, frame.y + 108, COL_BLUE, step == 1);
        // The arrow from p round to x, on the right of the boxes
        float sx = pBox.x + pBox.width, sy = pBox.y + pBox.height / 2;
        float ex = xBox.x + xBox.width, ey = xBox.y + xBox.height / 2;
        float bend = sx + 48;
        DrawSplineSegmentBezierCubic({ sx, sy }, { bend, sy }, { bend, ey }, { ex + 8, ey }, 2.5f, COL_BLUE);
        DrawLineEx({ ex + 2, ey }, { ex + 11, ey - 6 }, 2.5f, COL_BLUE);
        DrawLineEx({ ex + 2, ey }, { ex + 11, ey + 6 }, 2.5f, COL_BLUE);
    }

    // What the line does, and the output once printf has run
    static const char *notes[] = {
        "x gets a box on the stack, holding 5.",
        "p holds x's address: an arrow to x.",
        "*p = 7 follows the arrow and changes x.",
        "x really is 7 now. Output: 7",
    };
    ui_text_wrapped(notes[step], frame.x + 290, frame.y + 44, frame.width - 304, 14,
                    step == 3 ? COL_GREEN : COL_TEXT_SOFT);
}

void screen_title(App &app)
{
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) { open_map(app); return; }
    if (IsKeyPressed(KEY_E)) { open_exam_prep(app); return; }
    if (IsKeyPressed(KEY_V)) { open_versus(app); return; }
    if (IsKeyPressed(KEY_P)) { app.screen = Screen::Profile; return; }
    if (IsKeyPressed(KEY_ESCAPE)) { app.quit = true; return; }

    // Left: the logo, the name with a blinking cursor, and a line about it
    float x = 96, y = 112;
    ui_logo(x, y, 132);
    y += 160;

    float nameSize = 56;
    ui_text("STACK", x, y, nameSize, COL_TEXT, FontKind::MonoBold);
    float nx = x + ui_text_width("STACK", nameSize, FontKind::MonoBold);
    bool cursorOn = fmod(GetTime(), 1.0) < 0.6;
    ui_text("_", nx, y, nameSize, cursorOn ? COL_GREEN : COL_BG, FontKind::MonoBold);
    nx += ui_text_width("_", nameSize, FontKind::MonoBold);
    ui_text("TRACE", nx, y, nameSize, COL_TEXT, FontKind::MonoBold);
    y += 76;
    ui_text("Learn C and C++ by watching memory move.", x, y, 20, COL_TEXT_SOFT);
    y += 52;

    // Menu
    float bw = 236, bh = 48, gap = 14;
    if (ui_button("Start learning", { x, y, bw * 2 + gap, bh }, ButtonStyle::Primary, COL_GREEN)) { open_map(app); return; }
    y += bh + gap;
    if (ui_button("Exam prep", { x, y, bw, bh }, ButtonStyle::Outline, COL_PURPLE)) { open_exam_prep(app); return; }
    if (ui_button("Versus", { x + bw + gap, y, bw, bh }, ButtonStyle::Outline, COL_CORAL)) { open_versus(app); return; }
    y += bh + gap;
    if (ui_button("Profile", { x, y, bw, bh }, ButtonStyle::Outline, COL_AMBER)) { app.screen = Screen::Profile; return; }
    if (ui_button("Quit", { x + bw + gap, y, bw, bh }, ButtonStyle::Outline, COL_MUTED)) { app.quit = true; return; }
    y += bh + 28;

    // A welcome back line, once there's some progress to show
    const Progress &p = app.progress;
    if (!p.answers.empty()) {
        int streak = progress_streak(p, today_string());
        int maxPoints = (int)app.topics.size() * POINTS_MAX;
        std::string back = "Welcome back.  " + std::to_string(progress_total_points(p)) + " / " +
                           std::to_string(maxPoints) + " mastery points";
        if (streak > 0) back += "  |  " + std::to_string(streak) + (streak == 1 ? " day streak" : " day streak");
        ui_text(back, x, y, 14, COL_MUTED, FontKind::Mono);
    }

    draw_demo({ 700, 112, 484, 398 });

    // Along the bottom: who made it, and the keys
    float fy = SCREEN_H - FOOTER_H;
    DrawLine(0, (int)fy, SCREEN_W, (int)fy, COL_LINE_SOFT);
    ui_text("ENTER start   E exam prep   V versus   P profile   F11 full screen   ESC quit", MARGIN, fy + 20, 13,
            COL_MUTED, FontKind::Mono);
    const char *credit = "Advanced Programming, SETU  |  Ryan Mulcahy & Kobi Bourne";
    ui_text(credit, SCREEN_W - MARGIN - ui_text_width(credit, 13), fy + 19, 13, COL_MUTED);
}
