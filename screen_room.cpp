// screen_room.cpp - Mal's room. A dim room with a window, a wardrobe, a poster
// and a desk with an old computer. Click the computer and the camera zooms in
// until its screen fills yours: you're sitting at MAL-OS (mos.cpp), Mal's own
// operating system. From there you can poke around his files, play the
// minigames, write and compile some C, and dress Mal up.
//
// Outside the window grows your code tree: one branch per campaign region,
// one leaf per topic, gold when mastered.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

// ---------------------------------------------------------------------------
// State (how the room looks and what's open; MAL-OS itself lives in app.mos)
// ---------------------------------------------------------------------------

enum class View { Room, ZoomIn, Terminal, ZoomOut, Editor };

static View view = View::Room;
static float zoomT = 0;               // 0 = the room, 1 = the screen fills yours
static bool wardrobeOpen = false, treeOpen = false;
static std::string input;
static int historyPos = -1;
static int scrollUp = 0;              // lines scrolled back
static float revealLines = 0;         // boot messages appear a line at a time
static int pending = 0;               // 1 = compile next frame, 2 = run a.out next frame
static bool runAfterCompile = false;  // F5 in nano: compile, then run if it worked
static bool booted = false;
static EditBuffer editBuf;
static EditorView editView;
static int pokes = 0;

const Rectangle MONITOR_SCREEN = { 868, 300, 204, 140 };   // the computer's screen, in the room
const char *MOS_DIR = "practice_build/malos";
const char *HELLO_SAVE = "saves/mal_hello.c";

UnlockInfo unlock_info(const App &app)
{
    UnlockInfo u;
    u.level = level_for_xp(progress_xp(app.progress));
    u.bossesBeaten = (int)app.progress.bosses.size();
    u.topicsTotal = (int)app.topics.size();
    for (const Topic &t : app.topics)
        if (topic_mastery(app.progress, t.id) == Mastery::Mastered) u.topicsMastered++;
    auto crashes = app.progress.scores.find("crashes");   // game overs in MAL-OS games
    u.crashes = crashes == app.progress.scores.end() ? 0 : crashes->second;
    u.unlockAll = app.mos.unlockAll;
    u.owned = app.progress.owned;
    return u;
}

MosInfo mos_info(const App &app)
{
    MosInfo info;
    const char *n = getenv("USERNAME");
    if (!n || !*n) n = getenv("USER");
    info.user = (n && *n) ? n : "student";
    for (char &c : info.user) c = (char)tolower((unsigned char)c);
    UnlockInfo u = unlock_info(app);
    info.level = u.level;
    info.xp = progress_xp(app.progress);
    info.bossesBeaten = u.bossesBeaten;
    info.topicsMastered = u.topicsMastered;
    info.topicsTotal = u.topicsTotal;
    for (const Region &r : app.campaign) info.bossNames.push_back(r.bossName);
    info.scores = app.progress.scores;
    info.today = today_string();
    return info;
}

void wear_item(App &app, const WardrobeItem &item)
{
    if (!wardrobe_unlocked(item, unlock_info(app))) return;
    if (item.kind == WearKind::Hat) app.progress.hat = app.mal.hat = item.id;
    else if (item.kind == WearKind::Skin) app.progress.skin = app.mal.skin = item.id;
    else app.progress.pet = app.mal.pet = item.id;
    save_progress(app);
    sfx_play(Sfx::Correct);
    mal_react(app.mal, MalMood::Happy, 1.2f);
}

void open_room(App &app)
{
    app.screen = Screen::Room;
    view = View::Room;
    zoomT = 0;
    wardrobeOpen = treeOpen = false;
    if (app.mal.mood == MalMood::Freed) mal_react(app.mal, MalMood::Idle, 0);
    if (app.mos.helloSource.empty()) {   // hello.c, as the student last saved it
        std::ifstream f(HELLO_SAVE);
        std::stringstream ss;
        ss << f.rdbuf();
        app.mos.helloSource = f ? ss.str() : mos_default_hello();
    }
}

// ---------------------------------------------------------------------------
// The code tree: a branch per region, a leaf per topic
// ---------------------------------------------------------------------------

static void draw_leaf(Vector2 c, float r, Mastery m, float t)
{
    switch (m) {
        case Mastery::NotStarted: DrawCircleV(c, r * 0.45f, { 70, 84, 70, 255 }); break;   // a bud
        case Mastery::Attempted:  DrawCircleV(c, r * 0.7f, { 90, 150, 96, 255 }); break;
        case Mastery::Familiar:   DrawCircleV(c, r * 0.85f, { 110, 200, 120, 255 }); break;
        case Mastery::Proficient: DrawCircleV(c, r, COL_GREEN); break;
        case Mastery::Mastered: {   // a golden blossom
            for (int i = 0; i < 5; i++) {
                float a = i * 2 * PI / 5 + t * 0.4f;
                DrawCircleV({ c.x + cosf(a) * r * 0.6f, c.y + sinf(a) * r * 0.6f }, r * 0.55f, { 255, 205, 90, 255 });
            }
            DrawCircleV(c, r * 0.4f, COL_CORAL);
            break;
        }
    }
}

static void draw_star(Vector2 c, float r, Color col)
{
    Vector2 pts[10];
    for (int i = 0; i < 10; i++) {
        float a = -PI / 2 + i * PI / 5, rr = (i % 2 == 0) ? r : r * 0.45f;
        pts[i] = { c.x + cosf(a) * rr, c.y + sinf(a) * rr };
    }
    for (int i = 0; i < 10; i++) DrawTriangle(c, pts[(i + 1) % 10], pts[i], col);
}

static void draw_code_tree(const App &app, Rectangle a, float t, bool labels)
{
    float s = a.height / 260.0f;   // everything scales with the area
    Vector2 base = { a.x + a.width / 2, a.y + a.height - 14 * s };
    DrawEllipse((int)base.x, (int)(base.y + 4 * s), a.width * 0.42f, 12 * s, { 34, 62, 40, 255 });   // grass
    Vector2 top = { base.x, a.y + a.height * 0.38f };
    Color bark = { 110, 78, 52, 255 };
    DrawLineEx(base, top, 12 * s, bark);

    int regions = (int)app.campaign.size();
    for (int r = 0; r < regions; r++) {
        const Region &reg = app.campaign[r];
        float along = 0.25f + 0.55f * (r / (float)fmaxf(1, regions - 1));   // where it leaves the trunk
        Vector2 from = { base.x, base.y + (top.y - base.y) * along };
        float side = (r % 2 == 0) ? -1.0f : 1.0f;
        float sway = sinf(t * 0.8f + r) * 0.04f;
        float ang = side * (0.65f + 0.15f * (r / 2)) + sway;
        float len = a.height * (0.42f - 0.04f * r);
        Vector2 tip = { from.x + sinf(ang) * len, from.y - cosf(ang) * len };
        DrawLineEx(from, tip, (7 - r) * s, bark);

        int n = (int)reg.topics.size();
        for (int i = 0; i < n; i++) {
            float u = 0.25f + 0.75f * (i + 1) / (float)n;
            Vector2 p = { from.x + (tip.x - from.x) * u, from.y + (tip.y - from.y) * u };
            float off = (i % 2 == 0 ? 1 : -1) * 10 * s;
            Vector2 leaf = { p.x - cosf(ang) * off, p.y - sinf(ang) * off };
            DrawLineEx(p, leaf, 2 * s, bark);
            draw_leaf(leaf, 8 * s, topic_mastery(app.progress, reg.topics[i]), t + i);
        }
        if (progress_boss_beaten(app.progress, reg.id)) draw_star(tip, 11 * s, { 255, 220, 110, 255 });
        if (labels) {
            int done = 0;
            for (const std::string &id : reg.topics) if (topic_cleared(app.progress, id)) done++;
            std::string name = reg.name + "  " + std::to_string(done) + "/" + std::to_string(n) + " cleared";
            float tw = ui_text_width(name, 14, FontKind::UiBold);
            float lx = side < 0 ? tip.x - tw - 16 : tip.x + 16;
            ui_panel({ lx - 6, tip.y - 22, tw + 12, 24 }, Fade(COL_BG, 0.85f), Fade(COL_BG, 0), 0);
            ui_text(name, lx, tip.y - 19, 14, COL_TEXT, FontKind::UiBold);
        }
    }
    DrawCircleV(top, 8 * s, { 90, 170, 100, 255 });   // a tuft on top
}

// ---------------------------------------------------------------------------
// The room
// ---------------------------------------------------------------------------

enum class Thing { None, Computer, Wardrobe, Window, Mal };

static void glow_outline(Rectangle r, float t)
{
    float a = 0.5f + 0.3f * sinf(t * 5);
    DrawRectangleRoundedLinesEx({ r.x - 6, r.y - 6, r.width + 12, r.height + 12 }, 0.08f, 8, 3, Fade(COL_AMBER, a));
}

static void draw_room(App &app, float t, Thing hover)
{
    // Wall and floor
    DrawRectangleGradientV(0, 0, SCREEN_W, 520, { 30, 26, 44, 255 }, { 22, 20, 34, 255 });
    for (int x = 0; x < SCREEN_W; x += 64) DrawRectangle(x, 0, 2, 470, Fade(WHITE, 0.025f));   // wallpaper stripes
    DrawRectangle(0, 470, SCREEN_W, 50, { 18, 16, 26, 255 });
    DrawRectangleGradientV(0, 520, SCREEN_W, 200, { 46, 34, 28, 255 }, { 28, 22, 18, 255 });
    for (int i = 0; i < 4; i++) {   // floorboards
        int y = 552 + i * 44 + i * i * 4;
        DrawRectangle(0, y, SCREEN_W, 2, Fade(BLACK, 0.3f));
        for (int x = (i % 2) * 90; x < SCREEN_W; x += 180) DrawRectangle(x, y - (36 + i * 4), 2, 36 + i * 4, Fade(BLACK, 0.2f));
    }

    // Lamp light: warm glow on the wall and desk
    DrawCircleV({ 800, 360 }, 260, Fade(COL_AMBER, 0.05f));
    DrawCircleV({ 800, 400 }, 150, Fade(COL_AMBER, 0.05f));

    // ----- The window, with the code tree outside -----
    Rectangle win = { 80, 110, 250, 260 };
    if (hover == Thing::Window) glow_outline(win, t);
    DrawRectangleRec(win, { 70, 52, 38, 255 });
    Rectangle sky = { win.x + 12, win.y + 12, win.width - 24, win.height - 24 };
    DrawRectangleGradientV((int)sky.x, (int)sky.y, (int)sky.width, (int)sky.height, { 10, 18, 36, 255 }, { 22, 38, 64, 255 });
    for (int i = 0; i < 14; i++) {
        float tw = 0.5f + 0.5f * sinf(t * 2 + i);
        DrawCircleV({ sky.x + fmodf(i * 53.0f, sky.width), sky.y + fmodf(i * 31.0f, sky.height * 0.4f) + 6 }, 1.4f,
                    Fade(WHITE, 0.3f + 0.5f * tw));
    }
    ui_scissor((int)sky.x, (int)sky.y, (int)sky.width, (int)sky.height);
    draw_code_tree(app, { sky.x, sky.y + 10, sky.width, sky.height - 10 }, t, false);
    EndScissorMode();
    DrawRectangle((int)(win.x + win.width / 2 - 5), (int)win.y, 10, (int)win.height, { 70, 52, 38, 255 });   // the frame's bars
    DrawRectangle((int)win.x, (int)(win.y + win.height / 2 - 5), (int)win.width, 10, { 70, 52, 38, 255 });
    DrawRectangle((int)(win.x - 14), (int)(win.y + win.height), (int)(win.width + 28), 12, { 90, 66, 48, 255 });   // the sill
    for (int side = 0; side < 2; side++) {   // curtains, swaying very slightly
        float x = side == 0 ? win.x - 34 : win.x + win.width - 6;
        float sway = sinf(t * 0.7f + side) * 3;
        DrawRectangleRounded({ x + sway, win.y - 18, 40, win.height + 40 }, 0.3f, 6, { 110, 46, 56, 255 });
        for (int f = 1; f < 4; f++) DrawRectangle((int)(x + sway + f * 10), (int)(win.y - 10), 2, (int)(win.height + 26), Fade(BLACK, 0.2f));
    }
    DrawRectangle((int)(win.x - 50), (int)(win.y - 24), (int)(win.width + 100), 8, { 60, 60, 66, 255 });   // the rail

    // ----- The wardrobe -----
    Rectangle ward = { 380, 210, 160, 350 };
    if (hover == Thing::Wardrobe) glow_outline(ward, t);
    DrawRectangleRounded(ward, 0.04f, 6, { 64, 46, 34, 255 });
    DrawRectangleRounded({ ward.x + 10, ward.y + 30, ward.width / 2 - 14, ward.height - 60 }, 0.05f, 6, { 78, 56, 42, 255 });
    DrawRectangleRounded({ ward.x + ward.width / 2 + 4, ward.y + 30, ward.width / 2 - 14, ward.height - 60 }, 0.05f, 6, { 78, 56, 42, 255 });
    DrawCircleV({ ward.x + ward.width / 2 - 12, ward.y + 190 }, 5, COL_AMBER);
    DrawCircleV({ ward.x + ward.width / 2 + 12, ward.y + 190 }, 5, COL_AMBER);
    DrawRectangle((int)(ward.x - 8), (int)(ward.y - 10), (int)(ward.width + 16), 14, { 54, 38, 28, 255 });
    // A hat on top of it, waiting
    DrawTriangle({ ward.x + 104, ward.y - 10 }, { ward.x + 88, ward.y - 44 }, { ward.x + 72, ward.y - 10 }, COL_CORAL);
    DrawCircleV({ ward.x + 88, ward.y - 44 }, 4, COL_AMBER);

    // ----- The poster: our logo -----
    Rectangle poster = { 592, 118, 136, 168 };
    DrawRectangleRec({ poster.x + 4, poster.y + 4, poster.width, poster.height }, Fade(BLACK, 0.35f));
    DrawRectangleRec(poster, { 14, 18, 24, 255 });
    DrawRectangleLinesEx(poster, 2, { 48, 54, 61, 255 });
    ui_logo(poster.x + 18, poster.y + 14, 100);
    ui_text("STACK_TRACE", poster.x + 12, poster.y + 128, 15, COL_TEXT, FontKind::MonoBold);
    DrawCircleV({ poster.x + poster.width / 2, poster.y + 6 }, 4, COL_CORAL);   // a pin

    // ----- The shelf and books -----
    DrawRectangle(830, 214, 300, 10, { 88, 64, 44, 255 });
    Color books[] = { COL_CORAL, COL_BLUE, COL_PURPLE, COL_AMBER, COL_GREEN, { 160, 160, 170, 255 } };
    int bh[] = { 58, 50, 64, 46, 56, 52 };
    for (int i = 0; i < 6; i++) DrawRectangle(846 + i * 26, 214 - bh[i], 22, bh[i], books[i]);
    DrawRectangle(1010, 202, 92, 12, COL_TEXT_SOFT);   // a book lying flat
    ui_text("K&R", 1040, 202, 11, COL_BG, FontKind::MonoBold);

    // ----- The desk, computer, keyboard, mug and lamp -----
    DrawRectangle(760, 470, 440, 20, { 92, 66, 46, 255 });
    DrawRectangle(760, 490, 440, 26, { 70, 50, 36, 255 });
    DrawRectangle(776, 516, 16, 140, { 62, 44, 32, 255 });
    DrawRectangle(1168, 516, 16, 140, { 62, 44, 32, 255 });

    Rectangle monitor = { 846, 280, 248, 186 };
    if (hover == Thing::Computer) glow_outline(monitor, t);
    DrawRectangleRounded(monitor, 0.12f, 8, { 62, 66, 74, 255 });
    DrawRectangleRounded({ monitor.x + 8, monitor.y + 8, monitor.width - 16, monitor.height - 34 }, 0.12f, 8, { 40, 44, 50, 255 });
    DrawRectangleRec(MONITOR_SCREEN, { 5, 14, 7, 255 });
    // A few lines of the terminal, tiny, glowing
    {
        const std::vector<MosLine> &out = app.mos.out;
        int shown = 0;
        for (int i = (int)out.size() - 1; i >= 0 && shown < 7; i--, shown++)
            ui_text(out[i].text.substr(0, 34), MONITOR_SCREEN.x + 6, MONITOR_SCREEN.y + MONITOR_SCREEN.height - 22 - shown * 17, 10,
                    Fade(COL_GREEN, 0.75f), FontKind::Mono);
        if (out.empty()) ui_text("MAL-OS 1.0  _", MONITOR_SCREEN.x + 6, MONITOR_SCREEN.y + 8, 10, COL_GREEN, FontKind::Mono);
        DrawRectangleRec(MONITOR_SCREEN, Fade(COL_GREEN, 0.05f + 0.02f * sinf(t * 7)));
    }
    DrawCircleV({ monitor.x + monitor.width - 22, monitor.y + monitor.height - 14 }, 4, COL_GREEN);   // power light
    DrawRectangle(950, 466, 40, 8, { 54, 58, 64, 255 });
    DrawRectangleRounded({ 880, 472, 180, 10 }, 0.5f, 4, { 48, 52, 58, 255 });   // keyboard
    DrawRectangleRounded({ 1120, 430, 34, 40 }, 0.2f, 6, COL_TEXT_SOFT);         // mug
    DrawRing({ 1156, 450 }, 6, 10, -90, 90, 12, COL_TEXT_SOFT);
    ui_text("0x9000", 1121, 446, 8, COL_BG, FontKind::MonoBold);
    DrawLineEx({ 1130, 424 }, { 1134 + sinf(t * 2) * 3, 404 }, 2, Fade(WHITE, 0.15f));   // steam
    // Lamp
    DrawRectangle(780, 462, 44, 8, { 50, 54, 60, 255 });
    DrawLineEx({ 800, 462 }, { 790, 400 }, 5, { 50, 54, 60, 255 });
    DrawLineEx({ 790, 400 }, { 826, 368 }, 5, { 50, 54, 60, 255 });
    DrawTriangle({ 812, 356 }, { 824, 392 }, { 850, 370 }, { 70, 74, 82, 255 });
    DrawTriangle({ 832, 383 }, { 900, 470 }, { 840, 470 }, Fade(COL_AMBER, 0.07f));

    // ----- Mal -----
    Vector2 feet = { 680, 640 };
    if (hover == Thing::Mal) DrawEllipse((int)feet.x, (int)feet.y, 60, 14, Fade(COL_AMBER, 0.25f));
    mal_set_base(app.mal, MalMood::Idle);
    mal_draw_with_pet(app.mal, feet, 110);
}

static Thing thing_at(Vector2 m)
{
    if (CheckCollisionPointRec(m, { 846, 280, 248, 186 })) return Thing::Computer;
    if (CheckCollisionPointRec(m, { 380, 170, 160, 390 })) return Thing::Wardrobe;
    if (CheckCollisionPointRec(m, { 70, 100, 270, 290 })) return Thing::Window;
    if (CheckCollisionPointRec(m, { 620, 490, 120, 160 })) return Thing::Mal;
    return Thing::None;
}

// ---------------------------------------------------------------------------
// The terminal
// ---------------------------------------------------------------------------

static Color tone_color(MosTone tone)
{
    switch (tone) {
        case MosTone::Echo:   return { 190, 225, 195, 255 };
        case MosTone::Dim:    return { 90, 140, 100, 255 };
        case MosTone::Good:   return { 170, 255, 180, 255 };
        case MosTone::Bad:    return COL_CORAL;
        case MosTone::Lore:   return COL_PURPLE;
        case MosTone::Accent: return COL_AMBER;
        default:              return COL_GREEN;
    }
}

static void draw_crt_frame(const char *status)
{
    ClearBackground({ 30, 33, 38, 255 });
    Rectangle scr = { 24, 24, SCREEN_W - 48.0f, SCREEN_H - 48.0f };
    DrawRectangleRounded(scr, 0.03f, 10, { 5, 14, 7, 255 });
    DrawRectangleGradientV((int)scr.x, (int)scr.y, (int)scr.width, 80, Fade(COL_GREEN, 0.05f), Fade(COL_GREEN, 0));
    ui_text(status, 48, 36, 13, { 90, 140, 100, 255 }, FontKind::Mono);
    DrawLine(48, 60, SCREEN_W - 48, 60, { 22, 52, 28, 255 });
}

static void draw_scanlines(float t)
{
    for (int y = 24; y < SCREEN_H - 24; y += 3) DrawRectangle(24, y, SCREEN_W - 48, 1, Fade(BLACK, 0.22f));
    float bar = fmodf(t * 90, (float)SCREEN_H + 200) - 100;   // a slow roll bar
    DrawRectangle(24, (int)bar, SCREEN_W - 48, 60, Fade(COL_GREEN, 0.025f));
    DrawRectangleRoundedLinesEx({ 24, 24, SCREEN_W - 48.0f, SCREEN_H - 48.0f }, 0.03f, 10, 6, { 30, 33, 38, 255 });
}

// Whatever the shell asked for after a command.
static void do_action(App &app, const MosResult &r)
{
    switch (r.action) {
        case MosAction::RunGame:
            if (r.arg == "dungeon-won") {
                progress_record_score(app.progress, "dungeon", 1);
                save_progress(app);
                mal_react(app.mal, MalMood::Cheer, 2);
                sfx_play(Sfx::LevelUp);
            } else {
                start_game(app, r.arg);
            }
            break;
        case MosAction::Edit:
            eb_set_text(editBuf, app.mos.helloSource);
            editView = EditorView();
            view = View::Editor;
            break;
        case MosAction::Compile:    pending = 1; break;
        case MosAction::RunProgram: pending = 2; break;
        case MosAction::Wardrobe:   wardrobeOpen = true; break;
        case MosAction::Exit:       view = View::ZoomOut; break;
        default: break;
    }
}

static void terminal_input(App &app)
{
    MosInfo info = mos_info(app);
    int ch;
    while ((ch = GetCharPressed()) != 0)
        if (ch >= 32 && ch < 127 && input.size() < 120) { input += (char)ch; scrollUp = 0; }
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !input.empty()) input.pop_back();
    if (ctrl && IsKeyPressed(KEY_L)) { app.mos.out.clear(); input.clear(); }
    if (ctrl && IsKeyPressed(KEY_C)) { mos_print(app.mos, mos_prompt(app.mos, info) + input + "^C", MosTone::Echo); input.clear(); }
    if (IsKeyPressed(KEY_TAB)) input = mos_complete(app.mos, info, input);
    const std::vector<std::string> &hist = app.mos.history;
    if (IsKeyPressed(KEY_UP) && !hist.empty()) {
        historyPos = historyPos < 0 ? (int)hist.size() - 1 : (historyPos > 0 ? historyPos - 1 : 0);
        input = hist[historyPos];
    }
    if (IsKeyPressed(KEY_DOWN) && historyPos >= 0) {
        historyPos++;
        if (historyPos >= (int)hist.size()) { historyPos = -1; input.clear(); }
        else input = hist[historyPos];
    }
    scrollUp += (int)(GetMouseWheelMove() * 3);
    if (scrollUp < 0) scrollUp = 0;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        std::string line = input;
        input.clear();
        historyPos = -1;
        scrollUp = 0;
        sfx_play(Sfx::Click);
        do_action(app, mos_run(app.mos, info, line));
    }
}

static void run_pending(App &app)
{
    if (pending == 1) {
        std::string out;
        bool ok = compile_c_program(app.mos.helloSource, MOS_DIR, out);
        if (!out.empty()) mos_print(app.mos, out, ok ? MosTone::Dim : MosTone::Bad);
        app.mos.compiled = ok;
        if (ok) {
            mos_print(app.mos, runAfterCompile ? "compiled.  ./a.out" : "hello.c compiled. Run it with ./a.out", MosTone::Good);
            sfx_play(Sfx::Correct);
        } else {
            mos_print(app.mos, "gcc: compile failed. nano hello.c to fix it.", MosTone::Bad);
            sfx_play(Sfx::Wrong);
        }
        pending = (ok && runAfterCompile) ? 2 : 0;
        runAfterCompile = false;
        return;
    } else if (pending == 2) {
        std::string out = run_c_program(MOS_DIR);
        if (!out.empty() && out.back() == '\n') out.pop_back();
        mos_print(app.mos, out.empty() ? "(it printed nothing)" : out);
    }
    pending = 0;
}

static void draw_terminal(App &app, float t)
{
    MosInfo info = mos_info(app);
    std::string status = "MAL-OS 1.0   " + info.user + "   LV " + std::to_string(info.level) +
                         "      Tab completes   Up/Down history   ESC stand up";
    draw_crt_frame(status.c_str());

    const float lineH = 22, size = 16, x = 48, top = 72, bottom = SCREEN_H - 44.0f;
    int maxLines = (int)((bottom - top) / lineH) - 1;
    const std::vector<MosLine> &out = app.mos.out;
    int total = (int)out.size();
    if (revealLines > total) revealLines = (float)total;   // after clear, the output is shorter
    if (revealLines < total) revealLines = fminf((float)total, revealLines + GetFrameTime() * 40);
    int shownTotal = (int)revealLines;
    int maxScroll = shownTotal > maxLines ? shownTotal - maxLines : 0;
    if (scrollUp > maxScroll) scrollUp = maxScroll;
    int first = shownTotal - maxLines - scrollUp;
    if (first < 0) first = 0;
    float y = top;
    for (int i = first; i < shownTotal && i < first + maxLines; i++) {
        Color c = tone_color(out[i].tone);
        ui_text(out[i].text, x + 1, y + 1, size, Fade(c, 0.18f), FontKind::Mono);   // a soft glow
        ui_text(out[i].text, x, y, size, c, FontKind::Mono);
        y += lineH;
    }
    // The input line, with a blinking block cursor
    if (shownTotal >= total && scrollUp == 0) {
        std::string prompt = pending ? "" : mos_prompt(app.mos, info);
        std::string line = pending ? (pending == 1 ? "compiling..." : "running...") : prompt + input;
        ui_text(line, x, y, size, { 190, 225, 195, 255 }, FontKind::Mono);
        if (!pending && fmodf(t, 1.0f) < 0.55f)
            DrawRectangle((int)(x + ui_text_width(line, size, FontKind::Mono) + 2), (int)y + 2, 9, 18, COL_GREEN);
    } else if (scrollUp > 0) {
        ui_text("-- scrolled back: scroll down to type --", x, y, 13, { 90, 140, 100, 255 }, FontKind::Mono);
    }
    draw_scanlines(t);
}

static void draw_editor(App &app, float t)
{
    draw_crt_frame("nano  hello.c            Ctrl+S save    F5 save, compile and run    ESC save and close");
    editor_update_draw(editBuf, editView, { 40, 72, SCREEN_W - 80.0f, SCREEN_H - 120.0f }, true, false);
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool save = (ctrl && IsKeyPressed(KEY_S)) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F5);
    if (save) {
        app.mos.helloSource = eb_text(editBuf);
        app.mos.compiled = false;
        std::error_code ec;
        std::filesystem::create_directories("saves", ec);
        std::ofstream f(HELLO_SAVE);   // keep it between sessions
        if (f) f << app.mos.helloSource;
        if (ctrl && IsKeyPressed(KEY_S)) { mos_print(app.mos, "[ wrote hello.c ]", MosTone::Dim); sfx_play(Sfx::Click); }
    }
    if (IsKeyPressed(KEY_ESCAPE)) { view = View::Terminal; mos_print(app.mos, "[ saved hello.c ]", MosTone::Dim); }
    if (IsKeyPressed(KEY_F5)) {
        view = View::Terminal;
        mos_print(app.mos, "[ saved hello.c ]  gcc hello.c && ./a.out", MosTone::Dim);
        pending = 1;
        runAfterCompile = true;
    }
    draw_scanlines(t);
}

// ---------------------------------------------------------------------------
// The wardrobe and tree overlays
// ---------------------------------------------------------------------------

static void draw_wardrobe(App &app, float t)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.6f));
    Rectangle p = { 90, 60, SCREEN_W - 180.0f, SCREEN_H - 120.0f };
    ui_panel(p, COL_SURFACE, COL_AMBER, 2);
    ui_text("WARDROBE", p.x + 28, p.y + 22, 13, COL_AMBER, FontKind::MonoBold);
    ui_text("Dress Mal up. New things unlock as you level up, beat bosses and master topics.", p.x + 28, p.y + 42, 15, COL_TEXT_SOFT);

    // Mal, big, wearing the current outfit (and the pet beside them)
    Vector2 big = { p.x + 120, p.y + p.height - 70 };
    DrawEllipse((int)big.x, (int)big.y, 80, 18, COL_SURFACE_2);
    mal_draw_with_pet(app.mal, big, 150);

    // Bytes to spend, top right
    std::string have = std::to_string(app.progress.bytes) + " bytes";
    float hw = ui_text_width(have, 16, FontKind::MonoBold) + 28;
    ui_panel({ p.x + p.width - 28 - hw, p.y + 22, hw, 38 }, COL_BG, COL_PURPLE, 1);
    ui_text(have, p.x + p.width - 28 - hw + 14, p.y + 32, 16, COL_PURPLE, FontKind::MonoBold);

    // Tabs: hats, skins, pets, and the shop (things bought with bytes)
    static int tab = 0;
    const char *tabs[] = { "HATS & HAIR", "SKINS", "PETS", "SHOP" };
    const WearKind kinds[] = { WearKind::Hat, WearKind::Skin, WearKind::Pet, WearKind::Hat };
    const bool shop = tab == 3;
    float gx = p.x + 300, gy = p.y + 86;
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < 4; i++) {
        Rectangle tr = { gx + i * 150.0f, gy, 140, 34 };
        bool on = tab == i, hov = CheckCollisionPointRec(mouse, tr);
        ui_panel(tr, on ? COL_AMBER : (hov ? COL_SURFACE_2 : COL_BG), on ? COL_AMBER : COL_LINE, 1);
        float tw = ui_text_width(tabs[i], 12, FontKind::MonoBold);
        ui_text(tabs[i], tr.x + (tr.width - tw) / 2, tr.y + 10, 12, on ? COL_BG : COL_TEXT_SOFT, FontKind::MonoBold);
        if (hov) { ui_want_hand(); if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { tab = i; sfx_play(Sfx::Click); } }
    }
    if (IsKeyPressed(KEY_TAB)) tab = (tab + 1) % 4;
    const char *hint = tab == 0 ? "Reach the level, then buy it with bytes. Boss and mastery rewards are free."
                     : tab == 1 ? "Level skins cost bytes. Beat a boss and its look is yours for free."
                     : tab == 2 ? "Pets follow Mal around. Reach the level, then buy them with bytes."
                     : "Extras, just for bytes. Earn them from daily quests, MAL-OS high scores and Versus predictions.";
    ui_text(hint, gx, gy + 44, 13, COL_MUTED);
    gy += 72;

    UnlockInfo u = unlock_info(app);
    const float cw = 116, ch = 124, gap = 10;
    int col = 0;
    for (const WardrobeItem &item : wardrobe_items()) {
        bool shopOnly = item.price > 0 && item.level == 0;
        if (shop ? !shopOnly : (item.kind != kinds[tab] || shopOnly)) continue;   // shop-only items are in the SHOP tab
        if (col == 6) { col = 0; gy += ch + gap; }
        Rectangle c = { gx + col * (cw + gap), gy, cw, ch };
        col++;
        bool open = wardrobe_unlocked(item, u);
        bool worn = item.kind == WearKind::Hat ? app.mal.hat == item.id
                  : item.kind == WearKind::Skin ? app.mal.skin == item.id : app.mal.pet == item.id;
        bool earned = wardrobe_earned(item, u);
        bool forSale = item.price > 0 && !open && earned;   // reached, not bought yet
        bool canBuy = forSale && app.progress.bytes >= item.price;
        bool hover = (open || forSale) && CheckCollisionPointRec(mouse, c);
        ui_panel(c, hover ? COL_SURFACE_2 : COL_BG, worn ? COL_GREEN : forSale && hover ? COL_PURPLE : (hover ? COL_TEXT_SOFT : COL_LINE), worn || (forSale && hover) ? 2 : 1);

        if (item.kind == WearKind::Pet && item.id != "nopet") {
            mal_draw_pet(item.id, { c.x + cw / 2 - 4, c.y + 86 }, item.id == "duck" ? 56 : 66, t, false);
        } else {
            Mal preview;   // a little Mal showing the item
            preview.hat = item.kind == WearKind::Hat ? item.id : "none";
            preview.skin = item.kind == WearKind::Skin ? item.id : "classic";
            preview.clock = t;
            preview.blinkIn = 99;
            mal_draw(preview, { c.x + cw / 2 - (item.id == "shallow" ? 8 : 0), c.y + 86 }, 42);
        }
        if (forSale) {   // the price, and what clicking does
            std::string price = std::to_string(item.price) + " bytes";
            float pw = ui_text_width(price, 11, FontKind::MonoBold);
            ui_panel({ c.x + (cw - pw) / 2 - 6, c.y + 6, pw + 12, 18 }, canBuy ? COL_PURPLE : Fade(COL_PURPLE, 0.25f), Fade(COL_PURPLE, 0), 0);
            ui_text(price, c.x + (cw - pw) / 2, c.y + 9, 11, canBuy ? COL_BG : COL_PURPLE, FontKind::MonoBold);
            if (hover) {
                std::string act = canBuy ? "click to buy" : "need " + std::to_string(item.price - app.progress.bytes) + " more";
                ui_text(act, c.x + (cw - ui_text_width(act, 11, FontKind::Mono)) / 2, c.y + ch - 40, 11, canBuy ? COL_PURPLE : COL_MUTED, FontKind::Mono);
            }
        } else if (!open) {   // dimmed, with what unlocks it (and what it'll cost after)
            DrawRectangleRec(c, Fade(COL_BG, 0.72f));
            std::string req = wardrobe_requirement(item);
            float rw = ui_text_width(req, 11, FontKind::UiBold);
            ui_panel({ c.x + (cw - rw) / 2 - 6, c.y + 6, rw + 12, 18 }, Fade(COL_AMBER, 0.15f), Fade(COL_AMBER, 0), 0);
            ui_text(req, c.x + (cw - rw) / 2, c.y + 9, 11, COL_AMBER, FontKind::UiBold);
            if (item.price > 0) {
                std::string then = "then " + std::to_string(item.price) + " bytes";
                ui_text(then, c.x + (cw - ui_text_width(then, 10, FontKind::Mono)) / 2, c.y + 28, 10, COL_PURPLE, FontKind::Mono);
            }
        }
        float nw = ui_text_width(item.name, 12, FontKind::UiBold);
        ui_text(item.name, c.x + (cw - nw) / 2, c.y + ch - 22, 12, open ? COL_TEXT : COL_MUTED, FontKind::UiBold);
        if (hover) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (forSale) {
                    std::string why;
                    Vector2 mid = { c.x + cw / 2, c.y + ch / 2 };
                    if (wardrobe_buy(app.progress, item, u, why)) {   // bought: put it straight on
                        sfx_play(Sfx::Combo);
                        fx_burst(mid, 30, COL_PURPLE, COL_AMBER);
                        fx_float_text("-" + std::to_string(item.price) + " bytes", { mid.x, c.y - 6 }, 18, COL_PURPLE);
                        wear_item(app, item);
                        mal_react(app.mal, MalMood::Cheer, 2.0f);
                    } else {
                        sfx_play(Sfx::Wrong);
                        fx_float_text(why, { mid.x, c.y - 6 }, 16, COL_CORAL);
                    }
                } else {
                    wear_item(app, item);
                }
            }
        }
    }
    if (shop) {   // the const freeze: not something to wear, it protects the streak
        if (col == 6) { col = 0; gy += ch + gap; }
        Rectangle c = { gx + col * (cw + gap), gy, cw, ch };
        bool full = app.progress.freezes >= FREEZE_MAX, canBuy = !full && app.progress.bytes >= FREEZE_PRICE;
        bool hover = CheckCollisionPointRec(mouse, c);
        ui_panel(c, hover ? COL_SURFACE_2 : COL_BG, hover ? COL_BLUE : COL_LINE, hover ? 2 : 1);
        draw_freeze({ c.x + cw / 2 - 26, c.y + 34, 52, 52 }, true, t);
        std::string price = full ? "holding " + std::to_string(FREEZE_MAX) : std::to_string(FREEZE_PRICE) + " bytes";
        float pw = ui_text_width(price, 11, FontKind::MonoBold);
        ui_panel({ c.x + (cw - pw) / 2 - 6, c.y + 6, pw + 12, 18 }, canBuy ? COL_PURPLE : Fade(COL_PURPLE, 0.25f), Fade(COL_PURPLE, 0), 0);
        ui_text(price, c.x + (cw - pw) / 2, c.y + 9, 11, canBuy ? COL_BG : COL_PURPLE, FontKind::MonoBold);
        std::string held = "you have " + std::to_string(app.progress.freezes);
        ui_text(held, c.x + (cw - ui_text_width(held, 10, FontKind::Mono)) / 2, c.y + 90, 10, COL_BLUE, FontKind::Mono);
        float nw = ui_text_width("Const freeze", 12, FontKind::UiBold);
        ui_text("Const freeze", c.x + (cw - nw) / 2, c.y + ch - 22, 12, COL_TEXT, FontKind::UiBold);
        if (hover) {
            ui_want_hand();
            ui_text("Keeps your streak alive if you miss a day", c.x, c.y + ch + 6, 12, COL_BLUE);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                std::string why;
                Vector2 mid = { c.x + cw / 2, c.y + ch / 2 };
                if (streak_buy_freeze(app.progress, why)) {
                    save_progress(app);
                    sfx_play(Sfx::Combo);
                    fx_burst(mid, 30, COL_BLUE, WHITE);
                    fx_float_text("-" + std::to_string(FREEZE_PRICE) + " bytes", { mid.x, c.y - 6 }, 18, COL_PURPLE);
                } else {
                    sfx_play(Sfx::Wrong);
                    fx_float_text(why, { mid.x, c.y - 6 }, 16, COL_CORAL);
                }
            }
        }
    }
    if (ui_button("Done", { p.x + p.width - 28 - 140, p.y + p.height - 28 - 48, 140, 48 }, ButtonStyle::Primary, COL_GREEN) ||
        IsKeyPressed(KEY_ESCAPE))
        wardrobeOpen = false;
}

static void draw_tree_overlay(App &app, float t)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.6f));
    Rectangle p = { 140, 50, SCREEN_W - 280.0f, SCREEN_H - 100.0f };
    ui_panel(p, { 12, 22, 40, 255 }, COL_GREEN, 2);
    ui_text("YOUR CODE TREE", p.x + 28, p.y + 22, 13, COL_GREEN, FontKind::MonoBold);
    ui_text("A branch for each region, a leaf for each topic. Leaves grow as you learn; gold blossoms are Mastered,"
            " and stars mark beaten bosses.", p.x + 28, p.y + 42, 14, COL_TEXT_SOFT);
    draw_code_tree(app, { p.x + 40, p.y + 80, p.width - 80, p.height - 130 }, t, true);
    if (ui_button("Close", { p.x + p.width - 28 - 120, p.y + p.height - 28 - 44, 120, 44 }, ButtonStyle::Outline, COL_TEXT) ||
        IsKeyPressed(KEY_ESCAPE))
        treeOpen = false;
}

// ---------------------------------------------------------------------------

void screen_room(App &app)
{
    float t = (float)GetTime();
    float dt = GetFrameTime();
    Camera2D fit = ui_get_fit();

    if (view == View::ZoomIn) { zoomT += dt / 0.8f; if (zoomT >= 1) { zoomT = 1; view = View::Terminal; } }
    if (view == View::ZoomOut) { zoomT -= dt / 0.6f; if (zoomT <= 0) { zoomT = 0; view = View::Room; } }

    if (view == View::Terminal || view == View::Editor) {
        if (!booted) { mos_boot(app.mos, mos_info(app)); booted = true; revealLines = 0; }
        if (pending) run_pending(app);   // the frame before drew "compiling..."
        if (view == View::Editor) draw_editor(app, t);
        else {
            if (!wardrobeOpen) {
                if (IsKeyPressed(KEY_ESCAPE)) view = View::ZoomOut;
                else terminal_input(app);
            }
            draw_terminal(app, t);
        }
        if (wardrobeOpen) draw_wardrobe(app, t);
        return;
    }

    // ----- The room, zooming in on the computer when sitting down -----
    float e = zoomT * zoomT * (3 - 2 * zoomT);
    Vector2 centre = { MONITOR_SCREEN.x + MONITOR_SCREEN.width / 2, MONITOR_SCREEN.y + MONITOR_SCREEN.height / 2 };
    float z = 1 + e * (SCREEN_W / MONITOR_SCREEN.width - 1);
    Camera2D cam = {};
    cam.target = { SCREEN_W / 2.0f + (centre.x - SCREEN_W / 2.0f) * e, SCREEN_H / 2.0f + (centre.y - SCREEN_H / 2.0f) * e };
    cam.offset = { fit.offset.x + fit.zoom * SCREEN_W / 2.0f, fit.offset.y + fit.zoom * SCREEN_H / 2.0f };
    cam.zoom = fit.zoom * z;

    Thing hover = Thing::None;
    bool busy = view != View::Room || wardrobeOpen || treeOpen;
    if (!busy) hover = thing_at(GetMousePosition());

    // Clipping (the window's tree) has to follow the zoom too
    Camera2D clip = {};
    clip.zoom = cam.zoom;
    clip.offset = { cam.offset.x - cam.target.x * cam.zoom, cam.offset.y - cam.target.y * cam.zoom };
    ui_set_fit(clip);
    BeginMode2D(cam);
    draw_room(app, t, hover);
    EndMode2D();
    ui_set_fit(fit);
    BeginMode2D(fit);   // back to normal for everything on top
    if (zoomT > 0.6f) DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade({ 5, 14, 7, 255 }, (zoomT - 0.6f) / 0.4f));

    if (view == View::Room && !wardrobeOpen && !treeOpen) {
        // Title, hints and what's under the mouse
        ui_panel({ MARGIN, 18, 300, 54 }, Fade(COL_BG, 0.85f), COL_LINE, 1);
        ui_text("MAL'S ROOM", MARGIN + 16, 26, 12, COL_AMBER, FontKind::MonoBold);
        ui_text("Click the computer to sit down.", MARGIN + 16, 44, 14, COL_TEXT_SOFT);
        if (ui_button("Back", { SCREEN_W - MARGIN - 110, 18, 110, 44 }, ButtonStyle::Outline, COL_TEXT) || IsKeyPressed(KEY_ESCAPE)) {
            app.screen = Screen::Profile;
            return;
        }
        const char *label = hover == Thing::Computer ? "Use the computer"
                          : hover == Thing::Wardrobe ? "Open the wardrobe"
                          : hover == Thing::Window ? "Look at your code tree"
                          : hover == Thing::Mal ? "Poke Mal" : nullptr;
        if (label) {
            ui_want_hand();
            Vector2 mp = GetMousePosition();
            float w = ui_text_width(label, 14, FontKind::UiBold) + 20;
            ui_panel({ mp.x + 16, mp.y + 14, w, 28 }, Fade(COL_BG, 0.9f), COL_AMBER, 1);
            ui_text(label, mp.x + 26, mp.y + 19, 14, COL_TEXT, FontKind::UiBold);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                sfx_play(Sfx::Click);
                if (hover == Thing::Computer) view = View::ZoomIn;
                if (hover == Thing::Wardrobe) wardrobeOpen = true;
                if (hover == Thing::Window) treeOpen = true;
                if (hover == Thing::Mal) {
                    static const char *lines[] = { "Hey! Personal space! I'm a heap block!", "Go on, sit at the computer. I dare you.",
                                                   "Have you tried the wardrobe? I look great in hats.", "My tree's growing. Thanks to you." };
                    fx_float_text(lines[pokes++ % 4], { 680, 470 }, 18, COL_TEXT);
                    mal_react(app.mal, pokes % 3 == 0 ? MalMood::Cheer : MalMood::Happy, 1.2f);
                }
            }
        }
        DrawRectangle(0, SCREEN_H - FOOTER_H, SCREEN_W, FOOTER_H, Fade(COL_BG, 0.9f));
        ui_footer_hints({ "click the computer: MAL-OS", "the wardrobe: dress Mal up", "the window: your code tree", "ESC back" });
    }
    if (wardrobeOpen) draw_wardrobe(app, t);
    if (treeOpen) draw_tree_overlay(app, t);
}
