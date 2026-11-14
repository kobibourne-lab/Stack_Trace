// screen_bugdex.cpp - the Bugdex: a red handheld (like a Pokedex) with every
// C bug in it. Caught ones are in colour with their habitat, weakness and the
// line they live on; the rest are shadows until you catch them. Each bug has
// three quick fixes (spot the buggy line), and doing all three catches it.
// The bugs are in bugdex.cpp.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include <cmath>
#include <cstdio>

static int picked = 0;
static bool practising = false;
static int fix = 0;                // the quick fix showing (0..2)
static int wrongLine = 0;          // the last wrong click, to show it red
static double wrongAt = -100;
static bool fixRight = false;      // this fix has been answered right (showing why)
static double caughtAt = -100;     // the catch animation

void open_bugdex(App &app)
{
    if (app.screen != Screen::Bugdex) app.returnTo = app.screen;
    app.screen = Screen::Bugdex;
    practising = false;
}

static Color hex(unsigned int c, float alpha = 1)
{
    return Fade({ (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c, 255 }, alpha);
}

// A bug: a body shaped by its type, two eyes, and a mark that says what it is.
void draw_bug(const Bug &bug, Vector2 c, float size, float t, bool silhouette)
{
    Color body = silhouette ? Color{ 40, 44, 52, 255 } : hex(bug.colour);
    Color dark = silhouette ? Color{ 30, 33, 40, 255 } : Fade(BLACK, 0.25f);
    float s = size / 100.0f;
    float bob = sinf(t * 3 + bug.id.size()) * 3 * s;
    c.y += bob;
    switch (bug.shape) {
        case BugShape::Block:
            DrawRectangleRounded({ c.x - 46 * s, c.y - 34 * s, 92 * s, 68 * s }, 0.3f, 8, body);
            break;
        case BugShape::Blob:
            DrawCircleV(c, 40 * s, body);
            DrawCircleV({ c.x - 26 * s, c.y + 30 * s }, 12 * s, body);
            DrawCircleV({ c.x + 24 * s, c.y + 32 * s }, 10 * s, body);
            break;
        case BugShape::Drop:
            DrawCircleV({ c.x, c.y + 8 * s }, 36 * s, body);
            DrawTriangle({ c.x, c.y - 52 * s }, { c.x - 32 * s, c.y - 6 * s }, { c.x + 32 * s, c.y - 6 * s }, body);
            DrawCircleV({ c.x + 30 * s, c.y + 46 * s }, 8 * s, body);   // a drip
            break;
        case BugShape::Ghost: {
            DrawCircleV({ c.x, c.y - 8 * s }, 38 * s, body);
            DrawRectangle((int)(c.x - 38 * s), (int)(c.y - 8 * s), (int)(76 * s), (int)(38 * s), body);
            for (int i = 0; i < 4; i++) {   // the wavy hem
                float wx = c.x - 28.5f * s + i * 19 * s;
                DrawCircleV({ wx, c.y + 30 * s + sinf(t * 4 + i) * 3 * s }, 9.5f * s, body);
            }
            break;
        }
        case BugShape::Bolt:
            ui_triangle({ c.x + 14 * s, c.y - 56 * s }, { c.x - 30 * s, c.y + 8 * s }, { c.x + 6 * s, c.y + 4 * s }, body);
            ui_triangle({ c.x - 10 * s, c.y + 56 * s }, { c.x + 30 * s, c.y - 8 * s }, { c.x - 6 * s, c.y - 4 * s }, body);
            DrawRectangle((int)(c.x - 18 * s), (int)(c.y - 8 * s), (int)(36 * s), (int)(16 * s), body);
            break;
        case BugShape::Loop:
            DrawRing({ c.x - 22 * s, c.y }, 14 * s, 28 * s, 0, 360, 32, body);
            DrawRing({ c.x + 22 * s, c.y }, 14 * s, 28 * s, 0, 360, 32, body);
            break;
    }
    if (silhouette) return;
    // Eyes (on the body's middle), looking around a little
    float look = sinf(t * 1.3f + bug.colour) * 3 * s;
    float ey = bug.shape == BugShape::Ghost ? c.y - 12 * s : c.y - 4 * s;
    float ex = bug.shape == BugShape::Loop ? 22 * s : 13 * s;
    for (int side = -1; side <= 1; side += 2) {
        Vector2 e = { c.x + side * ex, ey };
        if (bug.shape == BugShape::Ghost) {   // x eyes: it's dead memory
            DrawLineEx({ e.x - 6 * s, e.y - 6 * s }, { e.x + 6 * s, e.y + 6 * s }, 3 * s, dark);
            DrawLineEx({ e.x - 6 * s, e.y + 6 * s }, { e.x + 6 * s, e.y - 6 * s }, 3 * s, dark);
        } else {
            DrawCircleV(e, 9 * s, WHITE);
            DrawCircleV({ e.x + look, e.y + 1 * s }, 4.5f * s, { 20, 20, 24, 255 });
        }
    }
    if (bug.id == "offbyone") {   // its third eye: the one too many
        DrawCircleV({ c.x + 36 * s, c.y - 20 * s }, 7 * s, WHITE);
        DrawCircleV({ c.x + 36 * s + look * 0.6f, c.y - 19 * s }, 3.5f * s, { 20, 20, 24, 255 });
    }
}

// The red handheld around everything.
static void draw_frame(float t)
{
    DrawRectangleRounded({ 24, 20, SCREEN_W - 48.0f, SCREEN_H - 40.0f }, 0.04f, 10, { 178, 34, 34, 255 });
    DrawRectangleRounded({ 24, 20, SCREEN_W - 48.0f, 12 }, 1, 4, Fade(WHITE, 0.12f));
    DrawRectangleRounded({ 44, 40, SCREEN_W - 88.0f, SCREEN_H - 80.0f }, 0.03f, 10, { 17, 21, 28, 255 });
    DrawCircleV({ 78, 72 }, 20, WHITE);
    DrawCircleV({ 78, 72 }, 16, Fade(COL_BLUE, 0.85f + 0.15f * sinf(t * 3)));
    DrawCircleV({ 72, 66 }, 5, Fade(WHITE, 0.6f));
    Color dots[] = { COL_CORAL, COL_AMBER, COL_GREEN };
    for (int i = 0; i < 3; i++) DrawCircleV({ 116.0f + i * 22, 62 }, 7, dots[i]);
}

static void start_practice(App &app)
{
    const Bug &b = bugdex_bugs()[picked];
    int all = (1 << (int)b.fixes.size()) - 1;
    if (app.progress.bugFixes[b.id] == all) app.progress.bugFixes[b.id] = 0;   // all done before: practise them again
    fix = 0;
    while (fix < (int)b.fixes.size() - 1 && bugdex_fix_done(app.progress, b.id, fix)) fix++;
    fixRight = false;
    wrongLine = 0;
    practising = true;
}

static void draw_practice(App &app, float t)
{
    const Bug &b = bugdex_bugs()[picked];
    bool caught = bug_caught(app.progress, b.id);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.65f));
    Rectangle p = { 170, 80, SCREEN_W - 340.0f, SCREEN_H - 160.0f };
    ui_panel(p, COL_SURFACE, COL_AMBER, 2);

    float age = (float)(GetTime() - caughtAt);
    if (age < 3.0f) {   // caught! the bug shrinks into a { } and it snaps shut
        float k = fminf(1, age / 0.9f);
        Vector2 c = { p.x + p.width / 2, p.y + 210 };
        float closeIn = 120 * (1 - k) + 26;
        ui_text("{", c.x - closeIn - 30, c.y - 46, 80, COL_AMBER, FontKind::MonoBold);
        ui_text("}", c.x + closeIn - 10, c.y - 46, 80, COL_AMBER, FontKind::MonoBold);
        if (k < 1) draw_bug(b, c, 120 * (1 - k * 0.8f), t, false);
        else DrawCircleV(c, 8 + 4 * sinf(t * 10), COL_GREEN);
        const char *line = "CAUGHT!";
        ui_text(line, c.x - ui_text_width(line, 40, FontKind::MonoBold) / 2, p.y + 320, 40, COL_GREEN, FontKind::MonoBold);
        std::string sub = b.name + " is in your Bugdex.";
        ui_text(sub, c.x - ui_text_width(sub, 18, FontKind::UiBold) / 2, p.y + 372, 18, COL_TEXT, FontKind::UiBold);
        if (age > 1.2f && (ui_button("Back to the Bugdex", { c.x - 110, p.y + 430, 220, 46 }, ButtonStyle::Primary, COL_GREEN) ||
                           IsKeyPressed(KEY_ENTER))) {
            caughtAt = -100;
            practising = false;
        }
        return;
    }

    ui_text("QUICK FIX " + std::to_string(fix + 1) + " OF " + std::to_string(b.fixes.size()), p.x + 28, p.y + 22, 13, COL_AMBER, FontKind::MonoBold);
    ui_text("Find the " + std::string(caught ? b.name : "bug") + ": click the line it's on.", p.x + 28, p.y + 44, 20, COL_TEXT, FontKind::UiBold);
    for (int i = 0; i < (int)b.fixes.size(); i++)   // progress pips
        DrawCircleV({ p.x + p.width - 90 + i * 24.0f, p.y + 32 }, 7, bugdex_fix_done(app.progress, b.id, i) ? COL_GREEN : COL_LINE);
    draw_bug(b, { p.x + p.width - 110, p.y + 140 }, 70, t, !caught);

    const BugFix &f = b.fixes[fix];
    Rectangle codeBox = { p.x + 28, p.y + 90, p.width - 220, 40.0f + 28 * f.code.size() };
    bool wrongShowing = GetTime() - wrongAt < 1.2;
    int highlight = fixRight ? f.line : (wrongShowing ? wrongLine : 0);
    int clicked = ui_code(f.code, codeBox, highlight, fixRight ? COL_GREEN : COL_CORAL, !fixRight, 17);
    if (clicked && !fixRight) {
        bool caughtNow = false;
        if (bugdex_answer_fix(app.progress, b.id, fix, clicked, today_string(), caughtNow)) {
            fixRight = true;
            sfx_play(Sfx::Correct);
            fx_burst({ codeBox.x + 80, codeBox.y + 20 + (f.line - 0.5f) * 26 }, 24, COL_GREEN, COL_AMBER);
            learned_today(app);
            quest_event(app, "bugfix");   // saves
            if (caughtNow) {
                caughtAt = GetTime();
                sfx_play(Sfx::LevelUp);
                mal_react(app.mal, MalMood::Cheer, 3.0f);
                fx_burst({ p.x + p.width / 2, p.y + 210 }, 90, hex(b.colour), COL_AMBER);
            }
        } else {
            wrongLine = clicked;
            wrongAt = GetTime();
            sfx_play(Sfx::Wrong);
            fx_shake(0.25f);
        }
    }
    float y = codeBox.y + codeBox.height + 18;
    if (fixRight) {
        ui_text("Got it!", p.x + 28, y, 18, COL_GREEN, FontKind::UiBold);
        ui_text_wrapped(f.why, p.x + 28, y + 28, p.width - 56, 15, COL_TEXT_SOFT);
        bool last = true;
        for (int i = 0; i < (int)b.fixes.size(); i++) if (!bugdex_fix_done(app.progress, b.id, i)) last = false;
        std::string next = last ? "Done" : "Next fix";
        if (ui_button(next, { p.x + p.width - 28 - 150, p.y + p.height - 28 - 46, 150, 46 }, ButtonStyle::Primary, COL_GREEN) ||
            IsKeyPressed(KEY_ENTER)) {
            if (last) practising = false;
            else {
                do { fix = (fix + 1) % (int)b.fixes.size(); } while (bugdex_fix_done(app.progress, b.id, fix));
                fixRight = false;
                wrongLine = 0;
            }
        }
    } else if (wrongShowing) {
        ui_text("Not that line. Look again.", p.x + 28, y, 16, COL_CORAL, FontKind::UiBold);
    } else {
        ui_text_wrapped("Hint: it lives in " + b.habitat + ".", p.x + 28, y, p.width - 56, 14, COL_MUTED);
    }
    if (ui_button("Stop", { p.x + 28, p.y + p.height - 28 - 46, 110, 46 }, ButtonStyle::Outline, COL_TEXT) || IsKeyPressed(KEY_ESCAPE))
        practising = false;
}

void screen_bugdex(App &app)
{
    float t = (float)GetTime();
    const std::vector<Bug> &bugs = bugdex_bugs();
    if (!practising && IsKeyPressed(KEY_ESCAPE)) { app.screen = app.returnTo; return; }
    if (!practising) {
        if (IsKeyPressed(KEY_RIGHT)) picked = (picked + 1) % (int)bugs.size();
        if (IsKeyPressed(KEY_LEFT)) picked = (picked + (int)bugs.size() - 1) % (int)bugs.size();
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_UP)) picked = (picked + 7) % 14 < (int)bugs.size() ? (picked + 7) % 14 : picked;
    }

    draw_frame(t);
    ui_text("BUGDEX", 196, 54, 30, COL_TEXT, FontKind::MonoBold);
    ui_text("caught " + std::to_string(bugs_caught(app.progress)) + " of " + std::to_string(bugs.size()) + " bugs", 330, 66, 15, COL_MUTED,
            FontKind::Mono);
    if (!practising && ui_button("Back", { SCREEN_W - 70 - 110, 50, 110, 40 }, ButtonStyle::Outline, COL_TEXT)) { app.screen = app.returnTo; return; }

    // The cards: two rows of seven
    const float cw = 152, ch = 112, gap = 10, gx = 70, gy = 106;
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < (int)bugs.size(); i++) {
        const Bug &b = bugs[i];
        Rectangle c = { gx + (i % 7) * (cw + gap), gy + (i / 7) * (ch + gap), cw, ch };
        bool caught = bug_caught(app.progress, b.id);
        bool hover = !practising && CheckCollisionPointRec(mouse, c);
        ui_panel(c, hover ? COL_SURFACE_2 : COL_SURFACE, i == picked ? COL_AMBER : COL_LINE, i == picked ? 2 : 1);
        draw_bug(b, { c.x + cw / 2, c.y + 42 }, 46, t + i, !caught);
        std::string name = caught ? b.name : "???";
        char num[16];
        snprintf(num, sizeof num, "#%03d", i + 1);
        ui_text(name, c.x + (cw - ui_text_width(name, 13, FontKind::UiBold)) / 2, c.y + 76, 13, caught ? COL_TEXT : COL_MUTED, FontKind::UiBold);
        std::string tag = std::string(num) + (caught ? "  " + b.type : "");
        ui_text(tag, c.x + (cw - ui_text_width(tag, 10, FontKind::Mono)) / 2, c.y + 94, 10, COL_MUTED, FontKind::Mono);
        if (caught && app.progress.bugs[b.id].caught > 1) {
            std::string x = "x" + std::to_string(app.progress.bugs[b.id].caught);
            ui_text(x, c.x + cw - 10 - ui_text_width(x, 12, FontKind::MonoBold), c.y + 8, 12, COL_AMBER, FontKind::MonoBold);
        }
        if (hover) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { picked = i; sfx_play(Sfx::Click); }
        }
    }

    // The picked bug, in detail
    const Bug &b = bugs[picked];
    bool caught = bug_caught(app.progress, b.id);
    Rectangle d = { 70, gy + 2 * (ch + gap) + 6, SCREEN_W - 140.0f, 0 };
    d.height = SCREEN_H - 62 - d.y;
    ui_panel(d, COL_SURFACE, COL_AMBER, 2);
    draw_bug(b, { d.x + 130, d.y + d.height / 2 }, 150, t, !caught);
    float x = d.x + 270, y = d.y + 22;
    char num[16];
    snprintf(num, sizeof num, "#%03d", picked + 1);
    ui_text(num, x, y, 26, COL_AMBER, FontKind::MonoBold);
    ui_text(caught ? b.name : "???", x + 90, y, 26, caught ? COL_AMBER : COL_MUTED, FontKind::MonoBold);
    y += 38;
    std::string line2 = b.type + (caught ? "  |  caught " + std::to_string(app.progress.bugs[b.id].caught) + (app.progress.bugs[b.id].caught == 1 ? " time" : " times")
                                         : "  |  not caught yet");
    ui_text(line2, x, y, 14, COL_MUTED, FontKind::Mono);
    y += 32;
    ui_text("Habitat:  " + b.habitat, x, y, 15, COL_TEXT, FontKind::Mono);
    y += 26;
    if (caught) {
        ui_text("Weakness: " + b.weakness, x, y, 15, COL_TEXT, FontKind::Mono);
        y += 34;
        Rectangle box = { x, y, 520, 86 };
        ui_panel(box, COL_BG, COL_LINE_SOFT, 1);
        ui_text(b.bad, box.x + 14, box.y + 12, 14, COL_CORAL, FontKind::Mono);
        ui_text(b.good, box.x + 14, box.y + 36, 14, COL_GREEN, FontKind::Mono);
        const BugRecord &r = app.progress.bugs[b.id];
        ui_text("first caught: " + r.how + ", " + r.date, box.x + 14, box.y + 60, 12, COL_MUTED, FontKind::Mono);
    } else {
        ui_text("Weakness: ???", x, y, 15, COL_MUTED, FontKind::Mono);
        y += 34;
        ui_text_wrapped("Catch it to see its weakness and the line it lives on.", x, y, 520, 14, COL_MUTED);
    }

    // Right: how to catch it
    float rx = d.x + d.width - 360, ry = d.y + 26;
    ui_text("Catch it by:", rx, ry, 15, COL_TEXT, FontKind::UiBold);
    ry += 28;
    std::vector<std::string> ways = { "its 3 quick fixes (below)" };
    if (!b.boss.empty()) {
        for (const Region &reg : app.campaign) if (reg.bossLook == b.boss) ways.push_back("beating " + reg.bossName);
    }
    if (b.id == "segfault") ways.push_back("fixing a crash in Exam prep");
    if (b.id == "infloop") ways.push_back("fixing a timeout in Exam prep");
    for (const std::string &w : ways) {
        DrawCircleV({ rx + 6, ry + 8 }, 3, COL_MUTED);
        ui_text(w, rx + 18, ry, 14, COL_TEXT_SOFT);
        ry += 22;
    }
    ry += 8;
    for (int i = 0; i < (int)b.fixes.size(); i++)
        DrawCircleV({ rx + 8 + i * 22.0f, ry + 6 }, 7, bugdex_fix_done(app.progress, b.id, i) ? COL_GREEN : COL_LINE);
    Rectangle go = { rx, d.y + d.height - 26 - 48, 330, 48 };
    if (!practising && ui_button(caught ? "Practise against it (3 quick fixes)" : "Catch it (3 quick fixes)", go, ButtonStyle::Primary, COL_AMBER)) {
        sfx_play(Sfx::Click);
        start_practice(app);
    }

    if (practising) draw_practice(app, t);
    else ui_text("click a bug   <- -> browse   ESC back", 70, SCREEN_H - 54, 12, COL_MUTED, FontKind::Mono);
}
