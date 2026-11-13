// screen_daily.cpp - the Duolingo loop: the streak flame, const freezes and
// today's three quests with their chests. The rules are in daily.cpp; this
// file draws them, and has the hooks the other screens call (learned_today,
// quest_event) plus the streak banner and quest toast drawn over everything.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include <cmath>
#include <ctime>

static const Color FLAME_OUT = { 255, 96, 64, 255 };
static const Color FLAME_MID = { 255, 168, 60, 255 };
static const Color FLAME_IN = { 255, 232, 140, 255 };
static const Color ICE = { 150, 214, 255, 255 };
static const Color COL_GOLD = { 255, 205, 80, 255 };

static double openedAt[4] = { -100, -100, -100, -100 };   // when each chest was opened (for its burst of light)
static std::string prizeLine;                              // what the last chest had in it
static double prizeAt = -100;

// ---------------------------------------------------------------------------
// Drawing pieces (the start screen uses these too)
// ---------------------------------------------------------------------------

// One layer of the flame: a round bottom and a point that sways.
static void flame_layer(Vector2 base, float h, float r, float sway, Color c)
{
    Vector2 centre = { base.x, base.y - r };
    Vector2 tip = { base.x + sway, base.y - h };
    DrawCircleV(centre, r, c);
    ui_triangle({ base.x - r * 0.98f, centre.y + r * 0.12f }, { base.x + r * 0.98f, centre.y + r * 0.12f }, tip, c);
    ui_triangle({ base.x - r * 0.7f, centre.y - r * 0.7f }, { base.x + r * 0.7f, centre.y - r * 0.7f }, tip, c);
}

void draw_flame(Vector2 base, float height, float t, bool lit)
{
    float flick = lit ? 1 + 0.05f * sinf(t * 11) + 0.03f * sinf(t * 17.3f) : 1;
    float sway = lit ? height * 0.07f * sinf(t * 6.1f) : 0;
    float h = height * flick, r = height * 0.32f;
    if (!lit) {
        flame_layer(base, h, r, 0, COL_LINE);
        flame_layer({ base.x, base.y - r * 0.12f }, h * 0.62f, r * 0.62f, 0, COL_SURFACE_2);
        return;
    }
    for (int i = 3; i >= 1; i--)   // a soft glow, brightest in the middle
        DrawCircleV({ base.x, base.y - r * 1.2f }, r * (0.9f + 0.32f * i), Fade(FLAME_OUT, 0.035f + 0.01f * sinf(t * 5)));
    flame_layer(base, h, r, sway, FLAME_OUT);
    flame_layer({ base.x, base.y - r * 0.08f }, h * 0.72f, r * 0.72f, sway * 0.7f, FLAME_MID);
    flame_layer({ base.x, base.y - r * 0.16f }, h * 0.44f, r * 0.44f, sway * 0.4f, FLAME_IN);
    // Embers drifting up
    for (int i = 0; i < 3; i++) {
        float k = fmodf(t * 0.8f + i * 0.33f, 1.0f);
        Vector2 e = { base.x + sinf(t * 3 + i * 2.1f) * r * 0.7f, base.y - h * (0.6f + k * 0.7f) };
        DrawCircleV(e, fmaxf(1, height * 0.025f * (1 - k)), Fade(FLAME_MID, 1 - k));
    }
}

void draw_freeze(Rectangle r, bool held, float t)
{
    if (!held) {   // an empty slot
        ui_panel(r, COL_BG, COL_LINE, 1);
        float w = ui_text_width("empty", r.height * 0.18f, FontKind::Mono);
        ui_text("empty", r.x + (r.width - w) / 2, r.y + r.height * 0.42f, r.height * 0.18f, COL_LINE, FontKind::Mono);
        return;
    }
    float bob = sinf(t * 2.2f + r.x * 0.05f) * r.height * 0.03f;
    Rectangle c = { r.x, r.y + bob, r.width, r.height };
    DrawRectangleRounded(c, 0.22f, 6, Fade(ICE, 0.28f));
    DrawRectangleRoundedLinesEx(c, 0.22f, 6, 2, ICE);
    DrawRectangleRounded({ c.x + c.width * 0.12f, c.y + c.height * 0.12f, c.width * 0.18f, c.height * 0.5f }, 1, 4, Fade(WHITE, 0.35f));
    float size = r.height * 0.24f;
    float w = ui_text_width("const", size, FontKind::MonoBold);
    ui_text("const", c.x + (c.width - w) / 2, c.y + c.height * 0.38f, size, WHITE, FontKind::MonoBold);
    // a little sparkle
    float s = 0.5f + 0.5f * sinf(t * 4 + r.x);
    Vector2 sp = { c.x + c.width * 0.8f, c.y + c.height * 0.2f };
    DrawLineEx({ sp.x - 5 * s, sp.y }, { sp.x + 5 * s, sp.y }, 1.5f, WHITE);
    DrawLineEx({ sp.x, sp.y - 5 * s }, { sp.x, sp.y + 5 * s }, 1.5f, WHITE);
}

void draw_chest(Vector2 base, float width, int state, float t, bool golden, float openAge)
{
    Color wood = golden ? Color{ 214, 150, 40, 255 } : Color{ 150, 96, 54, 255 };
    Color dark = golden ? Color{ 150, 96, 20, 255 } : Color{ 96, 60, 34, 255 };
    Color band = golden ? Color{ 255, 236, 160, 255 } : COL_GOLD;
    if (state == 0) { wood = { 70, 66, 64, 255 }; dark = { 50, 48, 47, 255 }; band = { 110, 104, 96, 255 }; }

    float bodyH = width * 0.46f, lidH = width * 0.3f;
    float hop = state == 1 ? fabsf(sinf(t * 4)) * width * 0.08f : 0;   // ready: it bounces
    float x = base.x - width / 2, y = base.y - bodyH - hop;

    if (state == 1) DrawCircleV({ base.x, y }, width * (0.6f + 0.05f * sinf(t * 5)), Fade(golden ? COL_GOLD : COL_AMBER, 0.14f));
    if (state == 2 && openAge < 1.6f) {   // light pouring out, just after it opens
        float k = 1 - openAge / 1.6f;
        for (int i = 0; i < 9; i++) {
            float a = PI + PI * (i + 0.5f) / 9;
            Vector2 tip = { base.x + cosf(a) * width * 1.3f, y + sinf(a) * width * 1.3f };
            ui_triangle({ base.x - width * 0.06f, y }, { base.x + width * 0.06f, y }, tip, Fade(golden ? COL_GOLD : COL_AMBER, 0.35f * k));
        }
    }

    // The body, with its bands
    DrawEllipse((int)base.x, (int)base.y + 2, width * 0.55f, width * 0.08f, Fade(BLACK, 0.3f));
    DrawRectangleRounded({ x, y, width, bodyH }, 0.18f, 4, wood);
    DrawRectangle((int)(x + width * 0.16f), (int)y, (int)(width * 0.09f), (int)bodyH, band);
    DrawRectangle((int)(x + width * 0.75f), (int)y, (int)(width * 0.09f), (int)bodyH, band);
    DrawRectangle((int)x, (int)(y + bodyH - width * 0.06f), (int)width, (int)(width * 0.06f), dark);

    if (state == 2) {   // open: the lid tipped back, the inside dark
        DrawRectangleRounded({ x + width * 0.04f, y - lidH * 0.9f, width * 0.92f, lidH * 0.9f }, 0.3f, 4, dark);
        DrawRectangleRounded({ x + width * 0.1f, y - lidH * 0.7f, width * 0.8f, lidH * 0.55f }, 0.3f, 4, { 30, 20, 14, 255 });
        DrawRectangle((int)x, (int)y - 2, (int)width, (int)(width * 0.06f), band);
        return;
    }
    // Closed: the rounded lid and the lock
    DrawRectangleRounded({ x, y - lidH, width, lidH + 2 }, 0.5f, 6, wood);
    DrawRectangle((int)(x + width * 0.16f), (int)(y - lidH + 2), (int)(width * 0.09f), (int)lidH, band);
    DrawRectangle((int)(x + width * 0.75f), (int)(y - lidH + 2), (int)(width * 0.09f), (int)lidH, band);
    DrawRectangle((int)x, (int)y - 2, (int)width, (int)(width * 0.05f), dark);
    float lw = width * 0.18f;
    DrawRectangleRounded({ base.x - lw / 2, y - lw * 0.45f, lw, lw }, 0.3f, 4, band);
    DrawCircleV({ base.x, y + lw * 0.05f }, lw * 0.14f, dark);
}

// ---------------------------------------------------------------------------
// The hooks the other screens call
// ---------------------------------------------------------------------------

void learned_today(App &app)
{
    if (!streak_mark_active(app.progress, today_string())) return;
    save_progress(app);
    app.streakLeft = 3.2f;   // the flame banner (after any level-up banner)
}

static void after_quest_change(App &app, const std::vector<bool> &before)
{
    std::string today = today_string();
    std::vector<Quest> quests = daily_quests(today);
    for (int i = 0; i < (int)quests.size(); i++) {
        if (!before[i] && quest_done(app.progress, quests[i], today)) {
            app.toastText = quests[i].text;
            app.toastLeft = 4.2f;
            sfx_play(Sfx::Combo);
        }
    }
    save_progress(app);
}

static std::vector<bool> quests_before(const App &app)
{
    std::string today = today_string();
    std::vector<bool> done;
    for (const Quest &q : daily_quests(today)) done.push_back(quest_done(app.progress, q, today));
    return done;
}

void quest_event(App &app, const std::string &key, int n)
{
    std::vector<bool> before = quests_before(app);
    daily_add(app.progress, today_string(), key, n);
    after_quest_change(app, before);
}

void quest_best(App &app, const std::string &key, int value)
{
    std::vector<bool> before = quests_before(app);
    daily_best(app.progress, today_string(), key, value);
    after_quest_change(app, before);
}

// The streak banner (drops in like the level-up one) and the quest toast.
void draw_daily_overlays(App &app)
{
    float dt = GetFrameTime(), t = (float)GetTime();
    if (app.toastLeft > 0) {
        app.toastLeft -= dt;
        float age = 4.2f - app.toastLeft;
        float in = ease_out_back(fminf(1, age / 0.4f)), out = app.toastLeft < 0.35f ? 1 - app.toastLeft / 0.35f : 0;
        float w = 400, h = 78;
        float x = SCREEN_W - (w + 24) * in + (w + 40) * out, y = SCREEN_H - FOOTER_H - h - 16;
        ui_panel({ x, y + 4, w, h }, { 120, 84, 38, 255 }, { 120, 84, 38, 255 }, 0);
        ui_panel({ x, y, w, h }, COL_SURFACE_2, COL_AMBER, 2);
        draw_chest({ x + 44, y + h - 16 }, 50, 1, t, false, 0);
        ui_text("QUEST COMPLETE", x + 86, y + 12, 12, COL_AMBER, FontKind::MonoBold);
        ui_text(app.toastText, x + 86, y + 30, 16, COL_TEXT, FontKind::UiBold);
        ui_text(app.screen == Screen::Daily ? "Click its chest to open it." : "Open its chest in Daily quests (D on the start screen).",
                x + 86, y + 52, 12, COL_TEXT_SOFT);
    }

    if (app.streakLeft <= 0 || app.levelUpLeft > 0) return;   // the level-up banner goes first
    const float total = app.freezesUsed > 0 ? 4.5f : 3.2f;   // main.cpp starts the freeze one at 4.5
    app.streakLeft -= dt;
    float age = total - app.streakLeft;
    float in = ease_out_back(fminf(1, age / 0.45f));
    float out = app.streakLeft < 0.4f ? ease_out_cubic(1 - app.streakLeft / 0.4f) : 0;
    float w = 560, h = 118;
    float x = (SCREEN_W - w) / 2, y = -h + (h + 24) * in - (h + 40) * out;
    bool icy = app.freezesUsed > 0;
    Color edge = icy ? ICE : FLAME_OUT;
    ui_panel({ x, y + 6, w, h }, icy ? Color{ 40, 80, 120, 255 } : Color{ 130, 50, 30, 255 }, edge, 0);
    ui_panel({ x, y, w, h }, COL_SURFACE_2, edge, 2);
    int streak = progress_streak(app.progress, today_string());
    if (icy) {
        draw_freeze({ x + 30, y + 26, 70, 70 }, true, t);
        ui_text("CONST FREEZE USED", x + 130, y + 18, 15, ICE, FontKind::MonoBold);
        ui_text(std::to_string(streak) + " day streak, safe", x + 130, y + 40, 30, COL_TEXT, FontKind::MonoBold);
        std::string line = app.freezesUsed == 1 ? "You missed a day, but the freeze kept it alive."
                                                : "You missed " + std::to_string(app.freezesUsed) + " days; your freezes kept it alive.";
        ui_text(line, x + 130, y + 82, 15, COL_TEXT_SOFT, FontKind::UiBold);
        if (app.streakLeft <= 0) app.freezesUsed = 0;
        return;
    }
    draw_flame({ x + 66, y + h - 16 }, 84, t, true);
    ui_text(streak <= 1 ? "STREAK STARTED" : "STREAK EXTENDED", x + 130, y + 18, 15, FLAME_MID, FontKind::MonoBold);
    ui_text(std::to_string(streak) + (streak == 1 ? " day streak!" : " day streak!"), x + 130, y + 40, 32, COL_TEXT, FontKind::MonoBold);
    ui_text("Learn something tomorrow to keep it burning.", x + 130, y + 82, 15, COL_TEXT_SOFT, FontKind::UiBold);
}

// ---------------------------------------------------------------------------
// The screen
// ---------------------------------------------------------------------------

void open_daily(App &app)
{
    if (app.screen != Screen::Daily) app.dailyFrom = app.screen;
    daily_roll(app.progress, today_string());
    app.screen = Screen::Daily;
}

static std::string time_to_midnight()
{
    time_t now = time(nullptr);
    tm *lt = localtime(&now);
    int left = 24 * 3600 - (lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec);
    return std::to_string(left / 3600) + "h " + std::to_string(left % 3600 / 60) + "m";
}

// A small sign for what each quest is about.
static const char *quest_icon(const std::string &key)
{
    if (key == "correct") return "+1";
    if (key == "lesson") return "{ }";
    if (key == "run") return ">_";
    if (key == "answer") return "?";
    if (key == "combo") return "x5";
    if (key == "quiz") return "Q";
    if (key == "solve") return "</>";
    if (key == "game") return "OS";
    return "#";
}

static void open_chest(App &app, int index, Vector2 at)
{
    int bytes = 0;
    std::string item, why;
    if (!daily_claim(app.progress, today_string(), index, bytes, item, why)) {
        sfx_play(Sfx::Wrong);
        fx_float_text(why, { at.x, at.y - 70 }, 16, COL_CORAL);
        return;
    }
    save_progress(app);
    openedAt[index] = GetTime();
    sfx_play(index == GOLDEN_CHEST ? Sfx::LevelUp : Sfx::Combo);
    fx_burst({ at.x, at.y - 30 }, index == GOLDEN_CHEST ? 80 : 40, index == GOLDEN_CHEST ? COL_GOLD : COL_AMBER, COL_PURPLE);
    fx_float_text("+" + std::to_string(bytes) + " bytes", { at.x, at.y - 80 }, 22, COL_PURPLE);
    mal_react(app.mal, MalMood::Cheer, 2.5f);
    if (!item.empty()) {
        const WardrobeItem *w = wardrobe_find(item);
        prizeLine = "Inside: " + std::to_string(bytes) + " bytes and the " + (w ? w->name : item) + "! It's in your wardrobe.";
        fx_float_text((w ? w->name : item) + "!", { at.x, at.y - 116 }, 26, COL_GOLD);
    } else {
        prizeLine = "Inside: " + std::to_string(bytes) + " bytes.";
    }
    prizeAt = GetTime();
}

static void draw_streak_panel(App &app, Rectangle L, float t)
{
    const Progress &p = app.progress;
    std::string today = today_string();
    int streak = progress_streak(p, today);
    bool lit = progress_day_active(p, today);

    ui_panel(L, COL_SURFACE, COL_LINE, 1);
    ui_text("YOUR STREAK", L.x + 24, L.y + 20, 12, FLAME_MID, FontKind::MonoBold);

    // The big flame, the number, and Mal
    draw_flame({ L.x + 96, L.y + 214 }, 150, t, streak > 0 && lit);
    if (streak > 0 && !lit) draw_flame({ L.x + 96, L.y + 214 }, 150 * (0.55f + 0.05f * sinf(t * 3)), t, true);   // embers: alive, but low
    std::string n = std::to_string(streak);
    ui_text(n, L.x + 190, L.y + 82, 72, streak > 0 ? COL_TEXT : COL_MUTED, FontKind::MonoBold);
    ui_text(streak == 1 ? "day streak" : "day streak", L.x + 192, L.y + 160, 18, COL_TEXT_SOFT, FontKind::UiBold);
    mal_set_base(app.mal, lit ? MalMood::Happy : (streak > 0 ? MalMood::Idle : MalMood::Sleep));
    mal_draw_with_pet(app.mal, { L.x + L.width - 92, L.y + 214 }, 74);

    const char *line = lit ? "You've learned today. The flame's safe until midnight."
                     : streak > 0 ? "Burning low! Answer a question, finish a lesson or solve a problem to keep it."
                                  : "Learn something today to light the flame: a question, a lesson or a problem.";
    ui_text_wrapped(line, L.x + 24, L.y + 236, L.width - 48, 14, lit ? COL_GREEN : COL_AMBER, FontKind::UiBold);

    // This week: a flame for a day learned, an ice cube for a frozen one
    float wy = L.y + 300;
    ui_text("THIS WEEK", L.x + 24, wy, 11, COL_MUTED, FontKind::MonoBold);
    std::vector<int> week = streak_week(p, today);
    static const char *names[] = { "M", "T", "W", "T", "F", "S", "S" };
    float slot = (L.width - 48) / 7;
    for (int i = 0; i < 7; i++) {
        std::string d = date_add_days(today, i - 6);
        Vector2 c = { L.x + 24 + slot * (i + 0.5f), wy + 50 };
        bool isToday = i == 6;
        DrawCircleV(c, 21, isToday ? COL_SURFACE_2 : COL_BG);
        if (isToday) DrawCircleLinesV(c, 22, COL_AMBER);
        if (week[i] == 1) draw_flame({ c.x, c.y + 13 }, 30, t + i, true);
        else if (week[i] == 2) draw_freeze({ c.x - 13, c.y - 13, 26, 26 }, true, t);
        else DrawCircleV(c, 4, COL_LINE);
        const char *wd = names[date_weekday(d)];
        ui_text(wd, c.x - ui_text_width(wd, 12, FontKind::MonoBold) / 2, c.y + 28, 12, isToday ? COL_AMBER : COL_MUTED, FontKind::MonoBold);
    }

    // Const freezes: what's held, and buying another
    float fy = L.y + 410;
    DrawLine((int)L.x + 24, (int)fy - 12, (int)(L.x + L.width - 24), (int)fy - 12, COL_LINE_SOFT);
    ui_text("CONST FREEZES", L.x + 24, fy, 12, ICE, FontKind::MonoBold);
    ui_text("const int streak;  // can't drop for a day", L.x + 24, fy + 20, 13, COL_MUTED, FontKind::Mono);
    for (int i = 0; i < FREEZE_MAX; i++) draw_freeze({ L.x + 24 + i * 78.0f, fy + 46, 66, 66 }, i < p.freezes, t);
    ui_text_wrapped("Used up by itself if you miss a day.", L.x + 190, fy + 46, L.width - 214, 13, COL_TEXT_SOFT);
    bool full = p.freezes >= FREEZE_MAX, afford = p.bytes >= FREEZE_PRICE;
    std::string label = full ? "Holding the most" : "Buy one  " + std::to_string(FREEZE_PRICE) + " bytes";
    Rectangle buy = { L.x + 190, fy + 76, L.width - 214, 38 };
    if (ui_button(label, buy, full || !afford ? ButtonStyle::Disabled : ButtonStyle::Primary, ICE)) {
        std::string why;
        if (streak_buy_freeze(app.progress, why)) {
            save_progress(app);
            sfx_play(Sfx::Combo);
            fx_burst({ L.x + 24 + (p.freezes - 1) * 78.0f + 33, fy + 79 }, 30, ICE, WHITE);
            fx_float_text("-" + std::to_string(FREEZE_PRICE) + " bytes", { buy.x + buy.width / 2, buy.y - 8 }, 18, COL_PURPLE);
        }
    }
    if (!full && !afford)
        ui_text("need " + std::to_string(FREEZE_PRICE - p.bytes) + " more bytes", buy.x, buy.y + 44, 12, COL_MUTED, FontKind::Mono);
}

static void draw_quests(App &app, Rectangle R, float t)
{
    std::string today = today_string();
    std::vector<Quest> quests = daily_quests(today);
    ui_text("TODAY'S QUESTS", R.x, R.y, 12, COL_AMBER, FontKind::MonoBold);
    std::string resets = "new quests in " + time_to_midnight();
    ui_text(resets, R.x + R.width - ui_text_width(resets, 12, FontKind::Mono), R.y, 12, COL_MUTED, FontKind::Mono);

    Vector2 mouse = GetMousePosition();
    float y = R.y + 26, ch = 112;
    for (int i = 0; i <= QUEST_COUNT; i++) {
        bool golden = i == GOLDEN_CHEST;
        Rectangle c = { R.x, y, R.width, golden ? ch + 14 : ch };
        bool done = golden ? quests_done(app.progress, today) == QUEST_COUNT : quest_done(app.progress, quests[i], today);
        bool opened = chest_opened(app.progress, i, today);
        int state = opened ? 2 : done ? 1 : 0;
        Rectangle chestHit = { c.x + c.width - 140, c.y, 140, c.height };
        bool hover = state == 1 && CheckCollisionPointRec(mouse, chestHit);
        ui_panel(c, hover ? COL_SURFACE_2 : COL_SURFACE, golden ? (done ? COL_GOLD : Fade(COL_GOLD, 0.4f)) : (state == 1 ? COL_AMBER : COL_LINE),
                 golden || state == 1 ? 2 : 1);

        if (!golden) {
            const Quest &q = quests[i];
            int have = quest_progress(app.progress, q, today);
            // The sign, the quest, and a bar
            Vector2 ic = { c.x + 46, c.y + c.height / 2 };
            DrawCircleV(ic, 26, done ? Fade(COL_GREEN, 0.18f) : COL_BG);
            DrawCircleLinesV(ic, 26, done ? COL_GREEN : COL_LINE);
            const char *icon = done ? "OK" : quest_icon(q.key);
            ui_text(icon, ic.x - ui_text_width(icon, 15, FontKind::MonoBold) / 2, ic.y - 9, 15, done ? COL_GREEN : COL_TEXT_SOFT, FontKind::MonoBold);
            ui_text(q.text, c.x + 92, c.y + 22, 19, done ? COL_TEXT_SOFT : COL_TEXT, FontKind::UiBold);
            Rectangle bar = { c.x + 92, c.y + 62, c.width - 92 - 260, 14 };
            DrawRectangleRounded(bar, 1, 6, COL_BG);
            if (have > 0) DrawRectangleRounded({ bar.x, bar.y, bar.width * have / q.target, bar.height }, 1, 6, done ? COL_GREEN : COL_AMBER);
            std::string count = std::to_string(have) + " / " + std::to_string(q.target);
            ui_text(count, bar.x + bar.width + 14, bar.y - 1, 14, done ? COL_GREEN : COL_TEXT_SOFT, FontKind::MonoBold);
            std::string reward = "+" + std::to_string(q.reward) + " bytes";
            ui_text(reward, c.x + 92, c.y + 84, 12, COL_PURPLE, FontKind::Mono);
        } else {
            int n = quests_done(app.progress, today);
            ui_text("THE GOLDEN CHEST", c.x + 28, c.y + 20, 13, COL_GOLD, FontKind::MonoBold);
            ui_text(done ? "All three done. It's yours!" : "Finish all three quests to open it.", c.x + 28, c.y + 42, 19, COL_TEXT, FontKind::UiBold);
            bool item = !daily_golden(app.progress, today).itemId.empty();
            ui_text(std::to_string(GOLDEN_BYTES) + " bytes" + (item ? ", and something from the shop today..." : ""), c.x + 28, c.y + 74, 13,
                    COL_PURPLE, FontKind::Mono);
            for (int k = 0; k < QUEST_COUNT; k++)
                DrawCircleV({ c.x + c.width - 210 + k * 20.0f, c.y + 30 }, 6, k < n ? COL_GOLD : COL_LINE);
        }

        // The chest: click it when it's ready
        float age = (float)(GetTime() - openedAt[i]);
        draw_chest({ c.x + c.width - 70, c.y + c.height - 22 }, golden ? 92 : 74, state, t + i * 0.7f, golden, opened ? age : 99);
        if (state == 1) {
            const char *open = "OPEN";
            ui_text(open, c.x + c.width - 70 - ui_text_width(open, 12, FontKind::MonoBold) / 2, c.y + 8, 12, golden ? COL_GOLD : COL_AMBER, FontKind::MonoBold);
        }
        if (hover) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) open_chest(app, i, { c.x + c.width - 70, c.y + c.height - 40 });
        }
        y += c.height + 12;
    }
}

void screen_daily(App &app)
{
    float t = (float)GetTime();
    daily_roll(app.progress, today_string());
    if (IsKeyPressed(KEY_ESCAPE)) { app.screen = app.dailyFrom; return; }

    ui_header("Daily quests - keep the streak alive, open the chests");
    if (ui_button("Back", { SCREEN_W - MARGIN - 110, 12, 110, 40 }, ButtonStyle::Outline, COL_TEXT)) { app.screen = app.dailyFrom; return; }
    std::string have = std::to_string(app.progress.bytes) + " bytes";
    float hw = ui_text_width(have, 15, FontKind::MonoBold) + 28;
    ui_panel({ SCREEN_W - MARGIN - 110 - 14 - hw, 14, hw, 36 }, COL_BG, COL_PURPLE, 1);
    ui_text(have, SCREEN_W - MARGIN - 110 - 14 - hw + 14, 23, 15, COL_PURPLE, FontKind::MonoBold);

    float top = HEADER_H + 20, bottom = SCREEN_H - FOOTER_H - 14;
    draw_streak_panel(app, { MARGIN, top, 420, bottom - top }, t);
    draw_quests(app, { MARGIN + 444, top, SCREEN_W - 2 * MARGIN - 444, bottom - top }, t);

    if (GetTime() - prizeAt < 4) {
        float w = ui_text_width(prizeLine, 15, FontKind::UiBold);
        ui_text(prizeLine, MARGIN + 444 + (SCREEN_W - 2 * MARGIN - 444 - w) / 2, bottom - 4, 15, COL_GOLD, FontKind::UiBold);
    }
    ui_footer_hints({ "click a chest to open it", "quests reset at midnight", "const freezes: shop or here", "ESC back" });
}
