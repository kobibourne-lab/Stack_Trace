// screen_codex.cpp - the C Codex: a grid of entries you've found (cards with
// stars), and the one picked: what it does, a program you can edit and run
// right there, what it should print, and the mistakes people make with it.
// The entries are in codex.cpp.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include "editor.h"
#include <cmath>

static const char *CODEX_DIR = "practice_build/codex";

static int picked = -1;            // the entry shown (index into codex_entries)
static std::string search;
static int focus = 0;              // 0 nothing, 1 the search box, 2 the code
static float gridScroll = 0;
static EditBuffer code;
static EditorView codeView;
static int loadedFor = -1;         // the entry the editor holds
static int pending = 0;            // 1 = compile and run next frame (this frame shows "Running")
static std::string output;         // what the last run printed (or the compiler said)
static bool outputIsError = false;
static bool hasOutput = false;

void open_codex(App &app)
{
    if (app.screen != Screen::Codex) app.returnTo = app.screen;
    app.screen = Screen::Codex;
    focus = 0;
    if (picked < 0) {   // start on the first one found
        const std::vector<CodexEntry> &es = codex_entries();
        for (int i = 0; i < (int)es.size(); i++) if (codex_unlocked(app.progress, es[i])) { picked = i; break; }
        if (picked < 0) picked = 0;
    }
}

static void load_example(int i)
{
    eb_set_text(code, codex_entries()[i].example);
    codeView = EditorView();
    loadedFor = i;
    hasOutput = false;
    output.clear();
}

static void draw_stars(float x, float y, int stars, float size)
{
    for (int s = 0; s < 3; s++) {
        Vector2 c = { x + s * size * 1.3f, y };
        Color col = s < stars ? COL_GREEN : COL_LINE;
        Vector2 pts[10];
        for (int i = 0; i < 10; i++) {
            float a = -PI / 2 + i * PI / 5, rr = (i % 2 == 0) ? size / 2 : size / 4.5f;
            pts[i] = { c.x + cosf(a) * rr, c.y + sinf(a) * rr };
        }
        for (int i = 0; i < 10; i++) DrawTriangle(c, pts[(i + 1) % 10], pts[i], col);
    }
}

static std::string topic_title(const App &app, const std::string &id)
{
    for (const Topic &t : app.topics) if (t.id == id) return t.title;
    return id;
}

static void draw_grid(App &app, Rectangle area)
{
    const std::vector<CodexEntry> &es = codex_entries();
    std::vector<int> shown = codex_search(search);
    const float cw = 112, ch = 100, gap = 10;
    int cols = 3;
    float total = ((shown.size() + cols - 1) / cols) * (ch + gap);
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, area)) gridScroll -= GetMouseWheelMove() * 60;
    float maxScroll = fmaxf(0, total - area.height);
    gridScroll = fminf(fmaxf(gridScroll, 0), maxScroll);

    ui_scissor((int)area.x - 4, (int)area.y - 4, (int)area.width + 8, (int)area.height + 8);
    for (int k = 0; k < (int)shown.size(); k++) {
        int i = shown[k];
        const CodexEntry &e = es[i];
        Rectangle c = { area.x + (k % cols) * (cw + gap), area.y + (k / cols) * (ch + gap) - gridScroll, cw, ch };
        if (c.y + ch < area.y - 4 || c.y > area.y + area.height) continue;
        bool open = codex_unlocked(app.progress, e);
        bool hover = CheckCollisionPointRec(mouse, c) && CheckCollisionPointRec(mouse, area);
        bool on = i == picked;
        ui_panel(c, open ? (hover ? COL_SURFACE_2 : COL_SURFACE) : COL_BG, on ? COL_AMBER : (open ? COL_LINE : COL_LINE_SOFT), on ? 2 : 1);
        if (open) {
            float nw = ui_text_width(e.name, 15, FontKind::MonoBold);
            ui_text(e.name, c.x + (cw - nw) / 2, c.y + 26, 15, on ? COL_AMBER : COL_TEXT, FontKind::MonoBold);
            float tw = ui_text_width(e.tag, 11, FontKind::Mono);
            ui_text(e.tag, c.x + (cw - tw) / 2, c.y + 50, 11, COL_MUTED, FontKind::Mono);
            draw_stars(c.x + cw / 2 - 13, c.y + 80, codex_stars(app.progress, e), 10);
        } else {
            ui_text("?", c.x + cw / 2 - 9, c.y + 18, 40, COL_LINE, FontKind::MonoBold);
            std::string hint = topic_title(app, e.topic);
            ui_text_centered_wrapped(hint, c.x + cw / 2, c.y + 66, cw - 10, 10, COL_MUTED, 2, FontKind::Mono);
        }
        if (hover) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { picked = i; focus = 0; sfx_play(Sfx::Click); }
        }
    }
    EndScissorMode();
    if (shown.empty()) ui_text("Nothing matches \"" + search + "\"", area.x, area.y + 10, 14, COL_MUTED);
}

static void run_pending(App &app)
{
    if (pending != 2) return;
    pending = 0;
    const CodexEntry &e = codex_entries()[picked];
    std::string out;
    bool ok = compile_c_program(eb_text(code), CODEX_DIR, out, e.cpp);
    hasOutput = true;
    if (!ok) {
        output = out.empty() ? "It didn't compile." : out;
        outputIsError = true;
        sfx_play(Sfx::Wrong);
        return;
    }
    output = run_c_program(CODEX_DIR);
    while (!output.empty() && (output.back() == '\n' || output.back() == ' ')) output.pop_back();
    outputIsError = false;
    sfx_play(Sfx::Correct);
    quest_event(app, "codexrun");
}

static void draw_entry(App &app, Rectangle r)
{
    const CodexEntry &e = codex_entries()[picked];
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    float x = r.x + 28, y = r.y + 22, w = r.width - 56;

    if (!codex_unlocked(app.progress, e)) {   // locked: what unlocks it
        ui_text("?", x, y - 4, 44, COL_LINE, FontKind::MonoBold);
        ui_text("Not found yet", x, y + 56, 24, COL_TEXT, FontKind::UiBold);
        ui_text_wrapped("Start the topic \"" + topic_title(app, e.topic) + "\" (a lesson or a quiz question) to find this entry.",
                        x, y + 94, w - 200, 16, COL_TEXT_SOFT);
        mal_set_base(app.mal, MalMood::Idle);
        mal_draw_with_pet(app.mal, { r.x + r.width - 110, r.y + 220 }, 90);
        return;
    }
    if (loadedFor != picked) load_example(picked);

    ui_text(e.name, x, y, 30, COL_AMBER, FontKind::MonoBold);
    float nw = ui_text_width(e.name, 30, FontKind::MonoBold);
    ui_text(e.tag, x + nw + 14, y + 12, 13, COL_MUTED, FontKind::Mono);
    draw_stars(r.x + r.width - 80, y + 16, codex_stars(app.progress, e), 14);
    y += 44;
    ui_text(e.signature, x, y, 16, COL_TEXT, FontKind::Mono);
    y += 30;
    y += ui_text_wrapped(e.about, x, y, w, 15, COL_TEXT_SOFT) + 14;

    // The program (edit it!) and what it prints
    float boxH = 250;
    Rectangle ed = { x, y, w * 0.58f, boxH };
    Rectangle out = { x + w * 0.58f + 14, y, w * 0.42f - 14, boxH };
    bool overEd = CheckCollisionPointRec(GetMousePosition(), ed);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { if (overEd) focus = 2; }
    editor_update_draw(code, codeView, ed, focus == 2, false);
    if (overEd) ui_want_ibeam();

    ui_panel(out, COL_BG, COL_LINE, 1);
    ui_text("SHOULD PRINT", out.x + 14, out.y + 12, 11, COL_MUTED, FontKind::MonoBold);
    float oy = out.y + 32;
    oy += ui_text_wrapped(e.output, out.x + 14, oy, out.width - 28, 15, COL_TEXT_SOFT, FontKind::Mono) + 16;
    DrawLine((int)out.x + 14, (int)oy - 6, (int)(out.x + out.width - 14), (int)oy - 6, COL_LINE_SOFT);
    ui_text(pending ? "RUNNING..." : "YOUR RUN", out.x + 14, oy + 4, 11, pending ? COL_AMBER : COL_MUTED, FontKind::MonoBold);
    if (hasOutput && !pending) {
        bool same = !outputIsError && output == e.output;
        ui_scissor((int)out.x, (int)oy + 20, (int)out.width, (int)(out.y + out.height - oy - 24));
        ui_text_wrapped(output.empty() ? "(it printed nothing)" : output, out.x + 14, oy + 24, out.width - 28, outputIsError ? 12 : 15,
                        outputIsError ? COL_CORAL : (same ? COL_GREEN : COL_TEXT), FontKind::Mono);
        EndScissorMode();
    } else if (!pending) {
        ui_text_wrapped("Press Run. Change the code and run it again to see what changes.", out.x + 14, oy + 24, out.width - 28, 13, COL_MUTED);
    }
    y += boxH + 12;

    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    if ((ui_button("Run it  (F5)", { x, y, 170, 42 }, pending ? ButtonStyle::Disabled : ButtonStyle::Primary, COL_GREEN) ||
         IsKeyPressed(KEY_F5) || (ctrl && IsKeyPressed(KEY_ENTER))) && !pending) {
        pending = 1;
        sfx_play(Sfx::Click);
    }
    if (ui_button("Reset", { x + 182, y, 100, 42 }, ButtonStyle::Outline, COL_TEXT)) { eb_replace_all(code, e.example); hasOutput = false; }
    if (find_compiler().empty()) ui_text("No compiler found, so Run won't work here.", x + 300, y + 13, 13, COL_CORAL);
    y += 58;

    ui_text("Common mistakes", x, y, 15, COL_TEXT, FontKind::UiBold);
    y += 24;
    for (const std::string &m : e.mistakes) {
        ui_text("x", x, y, 14, COL_CORAL, FontKind::MonoBold);
        ui_text(m, x + 18, y, 14, COL_CORAL, FontKind::Mono);
        y += 22;
    }
    ui_text("from the topic: " + topic_title(app, e.topic), x, r.y + r.height - 30, 13, COL_MUTED, FontKind::Mono);
}

void screen_codex(App &app)
{
    if (pending == 2) run_pending(app);
    if (pending == 1) pending = 2;   // this frame draws "Running", the next one runs it

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (focus != 0) focus = 0;
        else { app.screen = app.returnTo; return; }
    }
    // Typing in the search box
    if (focus == 1) {
        for (int c = GetCharPressed(); c > 0; c = GetCharPressed())
            if (c >= 32 && c < 127 && search.size() < 20) { search += (char)c; gridScroll = 0; }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !search.empty()) search.pop_back();
    }

    // Header
    ui_text("THE C CODEX", MARGIN, 24, 30, COL_TEXT, FontKind::MonoBold);
    std::string found = std::to_string(codex_found(app.progress)) + " of " + std::to_string(codex_entries().size()) + " entries found";
    ui_text(found, MARGIN + 244, 36, 14, COL_MUTED, FontKind::Mono);
    Rectangle box = { MARGIN + 520, 22, 300, 40 };
    bool overBox = CheckCollisionPointRec(GetMousePosition(), box);
    if (overBox) ui_want_ibeam();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && overBox) focus = 1;
    ui_panel(box, COL_BG, focus == 1 ? COL_AMBER : COL_LINE, 1);
    bool caret = focus == 1 && fmod(GetTime(), 1.0) < 0.6;
    if (search.empty() && focus != 1) ui_text("search: malloc, string.h ...", box.x + 12, box.y + 12, 14, COL_MUTED, FontKind::Mono);
    else ui_text(search + (caret ? "_" : ""), box.x + 12, box.y + 12, 14, COL_TEXT, FontKind::Mono);
    if (ui_button("Back", { SCREEN_W - MARGIN - 110, 22, 110, 40 }, ButtonStyle::Outline, COL_TEXT)) { app.screen = app.returnTo; return; }

    float top = 84, bottom = SCREEN_H - FOOTER_H - 12;
    draw_grid(app, { MARGIN, top, 356, bottom - top });
    draw_entry(app, { MARGIN + 376, top, SCREEN_W - 2 * MARGIN - 376, bottom - top });
    ui_footer_hints({ "click a card", "click the code to edit it", "F5 run", "scroll the cards", "ESC back" });
}
