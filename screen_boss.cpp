// screen_boss.cpp - a campaign boss fight, in four parts:
//   intro:  You & Mal VS the boss, the rules and the reward
//   fight:  the boss's problem under exam conditions. Every test you pass is a
//           hit on the boss's HP; a wrong submit costs one of your 3 lives
//   won:    stars, stats, the XP and the next region opening
//   lost:   Mal gets free()'d, and where the boss got you
// The fight reuses the problem screen's pieces (the description, the editor
// and the test results), like Versus does. The rules are in campaign.cpp.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include <cmath>

enum { BOSS_INTRO, BOSS_FIGHT, BOSS_WON, BOSS_LOST };
const float HUD_H = 96;

static const Region &region(const App &app) { return app.campaign[app.boss.region]; }

static std::string two_digits(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }
static std::string clock_text(double seconds)
{
    if (seconds < 0) seconds = 0;
    int s = (int)seconds;
    return two_digits(s / 60) + ":" + two_digits(s % 60);
}

// Finds the boss's problem in the Exam prep sets. false if it's missing.
static bool find_problem(const App &app, const std::string &id, int &set, int &problem)
{
    for (int s = 0; s < (int)app.practice.size(); s++)
        for (int p = 0; p < (int)app.practice[s].problems.size(); p++)
            if (app.practice[s].problems[p].id == id) { set = s; problem = p; return true; }
    return false;
}

static void back_to_campaign(App &app)
{
    if (app.mal.mood == MalMood::Freed) mal_react(app.mal, MalMood::Idle, 0);
    app.screen = Screen::Campaign;
}

// ---------------------------------------------------------------------------
// Drawing the bosses: each is made of a few shapes, in coral, with a white
// flash for a moment after it's hit
// ---------------------------------------------------------------------------

static void eyes(Vector2 a, Vector2 b, float r, bool angry, Color c)
{
    DrawCircleV(a, r, c);
    DrawCircleV(b, r, c);
    if (angry) {
        DrawLineEx({ a.x - r * 1.6f, a.y - r * 2.2f }, { a.x + r * 1.2f, a.y - r * 1.2f }, r * 0.8f, c);
        DrawLineEx({ b.x + r * 1.6f, b.y - r * 2.2f }, { b.x - r * 1.2f, b.y - r * 1.2f }, r * 0.8f, c);
    }
}

static void dashed_rect(Rectangle r, float thick, Color c)
{
    const float dash = 7, gap = 5;
    for (float x = r.x; x < r.x + r.width; x += dash + gap) {
        float e = fminf(x + dash, r.x + r.width);
        DrawLineEx({ x, r.y }, { e, r.y }, thick, c);
        DrawLineEx({ x, r.y + r.height }, { e, r.y + r.height }, thick, c);
    }
    for (float y = r.y; y < r.y + r.height; y += dash + gap) {
        float e = fminf(y + dash, r.y + r.height);
        DrawLineEx({ r.x, y }, { r.x, e }, thick, c);
        DrawLineEx({ r.x + r.width, y }, { r.x + r.width, e }, thick, c);
    }
}

// The boss, about size wide, centred on c.
static void draw_boss(const std::string &look, Vector2 c, float size, float flash, bool beaten)
{
    Color col = beaten ? COL_MUTED : COL_CORAL;
    if (flash > 0) col = { (unsigned char)(col.r + (255 - col.r) * flash), (unsigned char)(col.g + (255 - col.g) * flash),
                           (unsigned char)(col.b + (255 - col.b) * flash), 255 };
    float s = size / 100.0f;
    Color dark = COL_BG;

    if (look == "dangling") {
        // A pointer box, its arrow aimed at a dashed frame that isn't there any more
        Rectangle box = { c.x - 50 * s, c.y - 12 * s, 34 * s, 26 * s };
        DrawRectangleRounded(box, 0.3f, 6, COL_SURFACE);
        DrawRectangleRoundedLinesEx(box, 0.3f, 6, 3 * s, col);
        eyes({ box.x + 10 * s, box.y + 13 * s }, { box.x + 24 * s, box.y + 13 * s }, 3 * s, true, col);
        DrawLineEx({ box.x + box.width, c.y + 1 * s }, { c.x + 4 * s, c.y + 1 * s }, 3 * s, col);
        ui_triangle({ c.x + 2 * s, c.y - 6 * s }, { c.x + 2 * s, c.y + 8 * s }, { c.x + 12 * s, c.y + 1 * s }, col);
        Rectangle gone = { c.x + 12 * s, c.y - 22 * s, 40 * s, 44 * s };
        dashed_rect(gone, 2.5f * s, col);
        for (float ex : { gone.x + 12 * s, gone.x + 28 * s }) {
            DrawLineEx({ ex - 4 * s, c.y - 10 * s }, { ex + 4 * s, c.y - 2 * s }, 2.5f * s, col);
            DrawLineEx({ ex + 4 * s, c.y - 10 * s }, { ex - 4 * s, c.y - 2 * s }, 2.5f * s, col);
        }
        DrawRectangleRec({ gone.x + 34 * s, gone.y + 50 * s, 6 * s, 6 * s }, Fade(col, 0.5f));
        DrawRectangleRec({ gone.x + 22 * s, gone.y + 58 * s, 5 * s, 5 * s }, Fade(col, 0.35f));
    } else if (look == "leaker") {
        // A blob of heap that drips
        DrawCircleV({ c.x, c.y - 6 * s }, 34 * s, col);
        DrawRectangleRounded({ c.x - 34 * s, c.y - 6 * s, 68 * s, 30 * s }, 0.5f, 8, col);
        DrawCircleV({ c.x - 20 * s, c.y + 30 * s }, 6 * s, col);
        DrawCircleV({ c.x + 6 * s, c.y + 36 * s }, 5 * s, col);
        DrawCircleV({ c.x + 24 * s, c.y + 30 * s }, 4 * s, Fade(col, 0.7f));
        DrawCircleV({ c.x + 6 * s, c.y + 43 * s }, 3 * s, Fade(col, 0.5f));
        eyes({ c.x - 12 * s, c.y - 4 * s }, { c.x + 12 * s, c.y - 4 * s }, 5 * s, true, dark);
        DrawLineEx({ c.x - 10 * s, c.y + 12 * s }, { c.x + 10 * s, c.y + 12 * s }, 3 * s, dark);
    } else if (look == "shallow") {
        // Two objects, one array: the copy is just an outline over the original
        Rectangle a = { c.x - 40 * s, c.y - 26 * s, 52 * s, 52 * s };
        Rectangle b = { c.x - 12 * s, c.y - 14 * s, 52 * s, 52 * s };
        DrawRectangleRounded(a, 0.25f, 6, col);
        eyes({ a.x + 16 * s, a.y + 22 * s }, { a.x + 36 * s, a.y + 22 * s }, 4 * s, true, dark);
        DrawRectangleRoundedLinesEx(b, 0.25f, 6, 3 * s, col);
        eyes({ b.x + 30 * s, b.y + 36 * s }, { b.x + 44 * s, b.y + 36 * s }, 3.5f * s, false, col);
    } else {
        // The Off-By-One: a block with a "+1" it can't stop adding
        Rectangle body = { c.x - 34 * s, c.y - 24 * s, 68 * s, 56 * s };
        DrawRectangleRounded(body, 0.25f, 6, col);
        eyes({ c.x - 14 * s, c.y }, { c.x + 14 * s, c.y }, 5 * s, true, dark);
        DrawLineEx({ c.x - 12 * s, c.y + 18 * s }, { c.x + 12 * s, c.y + 14 * s }, 3 * s, dark);
        ui_text("+1", c.x + 22 * s, c.y - 46 * s, 22 * s, col, FontKind::MonoBold);
    }
}

// ---------------------------------------------------------------------------
// Starting
// ---------------------------------------------------------------------------

void open_boss(App &app, int r)
{
    if (r < 0 || r >= (int)app.campaign.size() || !boss_unlocked(app.campaign, app.progress, r)) return;   // reach it through the campaign
    BossState &b = app.boss;
    b = BossState();
    b.region = r;
    b.phase = BOSS_INTRO;
    int set, problem;
    b.found = find_problem(app, app.campaign[r].bossProblem, set, problem);
    if (b.found) b.totalTests = count_tests(app.practice[set].problems[problem]);
    if (app.mal.mood == MalMood::Freed) mal_react(app.mal, MalMood::Idle, 0);
    app.screen = Screen::Boss;
}

static void start_fight(App &app)
{
    BossState &b = app.boss;
    int set, problem;
    if (!find_problem(app, region(app).bossProblem, set, problem)) return;
    open_problem(app, set, problem, true);   // exam mode: a timer, no hints, no saved draft
    app.screen = Screen::Boss;
    b.phase = BOSS_FIGHT;
    b.lives = BOSS_LIVES;
    b.livesLost = 0;
    b.bestPassed = 0;
    b.lastRunCount = 0;
    b.timeUp = false;
    b.banner.clear();
    b.xpBefore = progress_xp(app.progress);
    sfx_play(Sfx::Click);
    mal_react(app.mal, MalMood::Idle, 0);
}

// ---------------------------------------------------------------------------
// The fight
// ---------------------------------------------------------------------------

static void win(App &app)
{
    BossState &b = app.boss;
    Workspace &w = app.work;
    const Region &reg = region(app);
    double limit = w.endsAt - w.started;
    b.seconds = (w.stoppedAt > 0 ? w.stoppedAt : GetTime()) - w.started;
    b.stars = boss_stars(true, b.livesLost, b.seconds, limit);
    b.firstWin = progress_record_boss(app.progress, reg.id, today_string(), (int)b.seconds, b.stars);
    learned_today(app);
    std::string error;
    progress_save(app.progressPath, app.progress, error);
    b.xpGained = progress_xp(app.progress) - b.xpBefore;
    b.phase = BOSS_WON;
    b.phaseAt = GetTime();
    sfx_play(Sfx::Combo);
    mal_react(app.mal, MalMood::Cheer, 4.0f);
    fx_burst({ SCREEN_W - 300.0f, 200 }, 70, COL_AMBER, COL_GREEN);
    check_level_up(app);
}

static void lose(App &app, const std::string &why)
{
    BossState &b = app.boss;
    b.phase = BOSS_LOST;
    b.lostReason = why;
    b.phaseAt = GetTime();
    sfx_play(Sfx::Crumble);
    mal_react(app.mal, MalMood::Freed, 0);
    fx_shake(1.0f);
}

// What the last run did to the boss, or to you.
static void after_run(App &app)
{
    BossState &b = app.boss;
    Workspace &w = app.work;
    const RunResult &r = w.result;
    bool counts = r.status != RunStatus::CompileError && r.status != RunStatus::NoCompiler && r.ruleProblems.empty();
    if (b.totalTests < r.passed + r.failed) b.totalTests = r.passed + r.failed;   // in case the count was off

    if (w.accepted) {
        b.bestPassed = b.totalTests;
        win(app);
        return;
    }
    int hits = 0;
    if (counts && r.passed > b.bestPassed) {
        hits = r.passed - b.bestPassed;
        b.bestPassed = r.passed;
        b.banner = "Hit! " + region(app).bossName + " -" + std::to_string(hits) + " HP";
        b.bannerGood = true;
        b.hitAt = GetTime();
        fx_float_text("-" + std::to_string(hits) + " HP", { SCREEN_W - MARGIN - 260.0f, 112 }, 24, COL_GREEN);
        fx_shake(0.3f);
        sfx_play(Sfx::Correct);
        mal_react(app.mal, MalMood::Happy, 1.2f);
    } else if (!w.resultWasSubmit) {
        b.banner = r.status == RunStatus::CompileError ? "Compile error: no damage. Fix the first error and run again."
                 : !r.ruleProblems.empty() ? "That breaks a rule, so it doesn't count. See the test results."
                 : "No damage. It still passes " + std::to_string(r.passed) + " of " + std::to_string(b.totalTests) + ".";
        b.bannerGood = false;
    }
    b.bannerAt = GetTime();

    // A submit that isn't accepted: the boss hits back
    if (w.resultWasSubmit) {
        if (b.timeUp) { lose(app, "Time ran out before every test passed."); return; }
        b.lives--;
        b.livesLost++;
        b.hurtAt = GetTime();
        b.banner = hits > 0 ? "Hit! -" + std::to_string(hits) + " HP, but not every test passed: the boss hits back, -1 life."
                            : "The boss hit you: -1 life. Runs are free; only a wrong submit costs a life.";
        b.bannerGood = false;
        sfx_play(Sfx::Wrong);
        fx_shake(0.8f);
        mal_react(app.mal, MalMood::Sad, 1.6f);
        if (b.lives <= 0) lose(app, "Out of lives: three submits that weren't right.");
    }
}

static void draw_hud(App &app)
{
    BossState &b = app.boss;
    Workspace &w = app.work;
    const Region &reg = region(app);
    double now = GetTime();
    DrawRectangle(0, 0, SCREEN_W, (int)HUD_H, { 17, 22, 29, 255 });
    DrawLine(0, (int)HUD_H - 1, SCREEN_W, (int)HUD_H - 1, COL_LINE_SOFT);

    // Left: you and Mal, and your lives
    float hurt = (float)fmax(0.0, 1.0 - (now - b.hurtAt) / 0.5);
    mal_draw(app.mal, { MARGIN + 24 + (hurt > 0 ? sinf((float)now * 60) * 4 * hurt : 0), HUD_H - 8 }, 38);
    int level = level_for_xp(progress_xp(app.progress));
    ui_text("You & Mal", MARGIN + 70, 24, 16, COL_TEXT, FontKind::UiBold);
    ui_text("Lv " + std::to_string(level), MARGIN + 70 + ui_text_width("You & Mal ", 16, FontKind::UiBold), 26, 13, COL_AMBER, FontKind::Mono);
    ui_hearts(MARGIN + 72, 52, b.lives, BOSS_LIVES);

    // Middle: the clock
    std::string area = reg.name;
    for (char &c : area) c = (char)toupper((unsigned char)c);
    area += " - BOSS";
    ui_text(area, SCREEN_W / 2.0f - ui_text_width(area, 11, FontKind::Mono) / 2, 14, 11, COL_MUTED, FontKind::Mono);
    double left = w.endsAt - (w.stoppedAt > 0 ? w.stoppedAt : now);
    std::string t = clock_text(left);
    Color tc = left < 60 ? COL_CORAL : COL_AMBER;
    float tw = ui_text_width(t, 26, FontKind::MonoBold) + 36;
    ui_panel({ SCREEN_W / 2.0f - tw / 2, 32, tw, 44 }, COL_BG, tc, 1);
    ui_text(t, SCREEN_W / 2.0f - tw / 2 + 18, 41, 26, tc, FontKind::MonoBold);

    // Right: the boss's HP bar and the boss, which shakes when it's hit
    int hp = boss_hp(b.totalTests, b.bestPassed);
    float bx = SCREEN_W - MARGIN - 110 - 300, bw = 300;
    std::string hpText = "HP " + std::to_string(hp) + " / " + std::to_string(b.totalTests);
    ui_text(hpText, bx, 20, 13, COL_CORAL, FontKind::Mono);
    float nw = ui_text_width(reg.bossName, 16, FontKind::UiBold);
    ui_text(reg.bossName, bx + bw - nw, 17, 16, COL_TEXT, FontKind::UiBold);
    int n = b.totalTests > 0 ? b.totalTests : 1;
    float gap = 4, segW = (bw - gap * (n - 1)) / n;
    for (int i = 0; i < n; i++) {
        Rectangle seg = { bx + i * (segW + gap), 42, segW, 14 };
        if (i < hp) DrawRectangleRounded(seg, 0.3f, 4, COL_CORAL);
        else { DrawRectangleRounded(seg, 0.3f, 4, COL_LINE_SOFT); DrawRectangleRoundedLinesEx(seg, 0.3f, 4, 1, COL_LINE); }
    }
    const char *note = "Each test you pass is a hit";
    ui_text(note, bx + bw - ui_text_width(note, 12), 64, 12, COL_MUTED);
    float flash = (float)fmax(0.0, 1.0 - (now - b.hitAt) / 0.35);
    float shake = flash > 0 ? sinf((float)now * 70) * 6 * flash : 0;
    draw_boss(reg.bossLook, { SCREEN_W - MARGIN - 44 + shake, HUD_H / 2 + 4 }, 70, flash, false);
}

static void fight_frame(App &app)
{
    BossState &b = app.boss;
    Workspace &w = app.work;
    double now = GetTime();

    problem_run_pending(app);
    if (w.runCount != b.lastRunCount) {
        b.lastRunCount = w.runCount;
        after_run(app);
        if (b.phase != BOSS_FIGHT) return;
    }

    // Time's up: whatever is there gets submitted, and that's the last chance
    if (!b.timeUp && now >= w.endsAt) {
        b.timeUp = true;
        w.timeUp = true;
    }
    if (b.timeUp && !b.timeUpSubmitted && w.runPhase == 0) {
        b.timeUpSubmitted = true;
        problem_start_run(app, true);
    }

    bool locked = w.timeUp;
    bool cmd = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (!locked && ((cmd && IsKeyPressed(KEY_ENTER)) || IsKeyPressed(KEY_F5) || IsKeyPressed(KEY_F6)))
        problem_start_run(app, shift || IsKeyPressed(KEY_F6));

    // ESC twice to retreat, so one slip doesn't end the fight
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (now - b.escArmed < 3.0) { back_to_campaign(app); return; }
        b.escArmed = now;
    }

    draw_hud(app);

    float top = HUD_H + 14, bottom = SCREEN_H - FOOTER_H - 12;
    Rectangle desc = { MARGIN, top, 380, bottom - top };
    float rx = desc.x + desc.width + 16, rw = SCREEN_W - MARGIN - rx;
    Rectangle editorBox = { rx, top, rw, 300 };
    Rectangle console = { rx, editorBox.y + editorBox.height + 12, rw, bottom - (editorBox.y + editorBox.height + 12) };
    problem_draw_description(app, desc);
    problem_draw_editor(app, editorBox, locked);
    problem_draw_console(app, console);

    // What the last run did, across the bottom of the editor for a few seconds
    if (!b.banner.empty() && now - b.bannerAt < 6.0) {
        Rectangle bar = { editorBox.x + 1, editorBox.y + editorBox.height - 40, editorBox.width - 2, 39 };
        Color c = b.bannerGood ? COL_GREEN : COL_CORAL;
        DrawRectangleRec(bar, ColorAlphaBlend(COL_SURFACE, Fade(c, 0.22f), WHITE));
        ui_text(b.banner, bar.x + 16, bar.y + 11, 15, c, FontKind::UiBold);
    }

    // Footer
    bool escShown = now - b.escArmed < 3.0;
    ui_footer_hints({ "Ctrl/Cmd+Enter run", "Ctrl/Cmd+Shift+Enter attack", escShown ? "ESC again to retreat" : "ESC ESC retreat" });
    float by = SCREEN_H - FOOTER_H + 8;
    bool busy = locked || w.runPhase != 0;
    if (ui_button("Attack (submit)", { SCREEN_W - MARGIN - 170, by, 170, 40 }, busy ? ButtonStyle::Disabled : ButtonStyle::Primary, COL_CORAL))
        problem_start_run(app, true);
    if (ui_button("Run", { SCREEN_W - MARGIN - 170 - 12 - 100, by, 100, 40 }, busy ? ButtonStyle::Disabled : ButtonStyle::Outline, COL_TEXT))
        problem_start_run(app, false);

    problem_end_frame(app);
}

// ---------------------------------------------------------------------------
// The intro, and the result
// ---------------------------------------------------------------------------

static void card(Rectangle r, const std::string &big, Color bigColor, const std::string &text)
{
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    ui_text(big, r.x + 16, r.y + 12, 18, bigColor, FontKind::MonoBold);
    ui_text_wrapped(text, r.x + 16, r.y + 40, r.width - 32, 13, COL_TEXT_SOFT);
}

static void intro_frame(App &app)
{
    BossState &b = app.boss;
    const Region &reg = region(app);
    if (IsKeyPressed(KEY_ESCAPE)) { back_to_campaign(app); return; }
    if (IsKeyPressed(KEY_ENTER) && b.found) { start_fight(app); return; }

    std::string area = reg.name;
    for (char &c : area) c = (char)toupper((unsigned char)c);
    area += "  -  BOSS FIGHT";
    ui_text(area, SCREEN_W / 2.0f - ui_text_width(area, 13, FontKind::MonoBold) / 2, 34, 13, COL_CORAL, FontKind::MonoBold);
    std::string sub = progress_boss_beaten(app.progress, reg.id) ? "A rematch: beat your best for more stars."
                                                                 : "Every topic in " + reg.name + " is cleared. Time to face its boss.";
    ui_text(sub, SCREEN_W / 2.0f - ui_text_width(sub, 14) / 2, 56, 14, COL_MUTED);

    // You & Mal  VS  the boss
    float leftX = 310, rightX = SCREEN_W - 310, midY = 230;
    mal_set_base(app.mal, MalMood::Idle);
    mal_draw(app.mal, { leftX, midY + 50 }, 130);
    ui_text("You & Mal", leftX - ui_text_width("You & Mal", 24, FontKind::UiBold) / 2, midY + 66, 24, COL_TEXT, FontKind::UiBold);
    ui_hearts(leftX - 32, midY + 104, BOSS_LIVES, BOSS_LIVES);
    ui_text("VS", SCREEN_W / 2.0f - ui_text_width("VS", 56, FontKind::MonoBold) / 2, midY - 30, 56, COL_LINE, FontKind::MonoBold);
    float bob = sinf((float)GetTime() * 2) * 4;
    draw_boss(reg.bossLook, { rightX, midY - 10 + bob }, 150, 0, false);
    ui_text(reg.bossName, rightX - ui_text_width(reg.bossName, 24, FontKind::UiBold) / 2, midY + 66, 24, COL_TEXT, FontKind::UiBold);
    std::string hp = "HP " + std::to_string(b.totalTests);
    ui_text(hp, rightX - ui_text_width(hp, 15, FontKind::MonoBold) / 2, midY + 102, 15, COL_CORAL, FontKind::MonoBold);

    // The taunt
    float tw = 820, th = ui_text_wrapped_height("\"" + reg.taunt + "\"", tw - 48, 17) + 28;
    Rectangle taunt = { (SCREEN_W - tw) / 2, 372, tw, th };
    ui_panel(taunt, COL_SURFACE, COL_CORAL, 1);
    ui_text_centered_wrapped("\"" + reg.taunt + "\"", SCREEN_W / 2.0f, taunt.y + 14, tw - 48, 17, COL_TEXT, 3);

    // The rules
    if (!b.found) {
        ui_text("This boss's problem (" + reg.bossProblem + ") isn't in content/practice, so it can't be fought.",
                MARGIN, 480, 15, COL_CORAL);
    } else {
        int set = 0, problem = 0;
        find_problem(app, reg.bossProblem, set, problem);
        int minutes = app.practice[set].problems[problem].examMinutes;
        float cw = (SCREEN_W - 2 * MARGIN - 3 * 14) / 4.0f, cy = 470;
        std::string n = std::to_string(b.totalTests);
        card({ MARGIN, cy, cw, 92 }, std::to_string(minutes) + ":00", COL_AMBER, "On the clock, like the real coding exam.");
        card({ MARGIN + (cw + 14), cy, cw, 92 }, n + " tests = " + n + " HP", COL_CORAL, "Every test you pass is a hit on the boss.");
        card({ MARGIN + 2 * (cw + 14), cy, cw, 92 }, "-1 life", COL_CORAL, "For a wrong submit. Runs are free.");
        card({ MARGIN + 3 * (cw + 14), cy, cw, 92 }, "No hints", COL_TEXT, "Exam conditions. Three lives.");
    }

    // The reward, and the buttons
    float fy = SCREEN_H - 84;
    std::string reward;
    if (progress_boss_beaten(app.progress, reg.id)) {
        const BossRecord &rec = app.progress.bosses.at(reg.id);
        reward = "Your best: " + std::to_string(rec.stars) + (rec.stars == 1 ? " star in " : " stars in ") + clock_text(rec.seconds) +
                 ". More stars, more XP.";
    } else {
        reward = "Win: +" + std::to_string(XP_BOSS) + " XP, +" + std::to_string(XP_STAR) + " XP a star";
        if (b.region + 1 < (int)app.campaign.size()) reward += ", and " + app.campaign[b.region + 1].name + " opens";
    }
    ui_text("WIN", MARGIN, fy + 14, 12, COL_GREEN, FontKind::MonoBold);
    ui_text(reward, MARGIN + 44, fy + 12, 15, COL_TEXT_SOFT);
    if (ui_button("Fight", { SCREEN_W - MARGIN - 150, fy, 150, 48 }, b.found ? ButtonStyle::Primary : ButtonStyle::Disabled, COL_CORAL) && b.found) {
        start_fight(app);
        return;
    }
    if (ui_button("Back to map", { SCREEN_W - MARGIN - 150 - 12 - 150, fy, 150, 48 }, ButtonStyle::Outline, COL_TEXT)) back_to_campaign(app);
}

// A five-pointed star, filled or just outlined.
static void star(Vector2 c, float r, bool filled)
{
    Vector2 pts[10];
    for (int i = 0; i < 10; i++) {
        float a = -PI / 2 + i * PI / 5;
        float rr = i % 2 == 0 ? r : r * 0.45f;
        pts[i] = { c.x + cosf(a) * rr, c.y + sinf(a) * rr };
    }
    for (int i = 0; i < 10; i++) {
        if (filled) ui_triangle(c, pts[i], pts[(i + 1) % 10], COL_AMBER);
        else DrawLineEx(pts[i], pts[(i + 1) % 10], 2, COL_LINE);
    }
}

static void stat(Rectangle r, const std::string &big, Color c, const std::string &text)
{
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    ui_text(big, r.x + 16, r.y + 12, 22, c, FontKind::MonoBold);
    ui_text(text, r.x + 16, r.y + 44, 13, COL_TEXT_SOFT);
}

static void won_frame(App &app)
{
    BossState &b = app.boss;
    Workspace &w = app.work;
    const Region &reg = region(app);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) { back_to_campaign(app); return; }

    // Mal cheering, with the boss crumbled beside it
    mal_draw(app.mal, { 190, 250 }, 140);
    draw_boss(reg.bossLook, { 330, 232 }, 70, 0, true);
    float x = 440;
    ui_text("BOSS DEFEATED", x, 76, 14, COL_GREEN, FontKind::MonoBold);
    ui_text(reg.bossName, x, 100, 40, COL_TEXT, FontKind::UiBold);
    ui_text("has been freed.", x, 146, 40, COL_TEXT, FontKind::UiBold);
    for (int i = 0; i < 3; i++) star({ x + 18 + i * 42.0f, 222 }, 17, i < b.stars);
    const char *why = b.stars == 3 ? "Three stars: no lives lost, in under half the time."
                    : b.livesLost > 0 ? "Next time: win without losing a life for another star."
                    : "Next time: finish in under half the time for another star.";
    ui_text(why, x + 140, 213, 14, COL_TEXT_SOFT);

    float cw = (SCREEN_W - 2 * MARGIN - 3 * 14) / 4.0f, cy = 300;
    stat({ MARGIN, cy, cw, 76 }, clock_text(b.seconds), COL_TEXT, "Time (limit " + clock_text(w.endsAt - w.started) + ")");
    stat({ MARGIN + cw + 14, cy, cw, 76 }, std::to_string(b.totalTests) + " / " + std::to_string(b.totalTests), COL_GREEN, "Tests passed");
    stat({ MARGIN + 2 * (cw + 14), cy, cw, 76 }, std::to_string(w.runCount), COL_TEXT, w.runCount == 1 ? "Run to win" : "Runs to win");
    stat({ MARGIN + 3 * (cw + 14), cy, cw, 76 }, std::to_string(b.lives) + " / " + std::to_string(BOSS_LIVES), COL_CORAL, "Lives left");

    // What beat it: the student's own code
    Rectangle codeBox = { MARGIN, 392, 700, SCREEN_H - 392 - 96 };
    ui_panel(codeBox, COL_SURFACE, COL_GREEN, 1);
    ui_text("WHAT BEAT IT: YOUR CODE", codeBox.x + 16, codeBox.y + 12, 12, COL_GREEN, FontKind::MonoBold);
    ui_code(w.code.lines, { codeBox.x + 12, codeBox.y + 34, codeBox.width - 24, codeBox.height - 46 }, 0, COL_GREEN, false, 13);

    // The reward
    Rectangle xpBox = { MARGIN + 716, 392, SCREEN_W - 2 * MARGIN - 716, codeBox.height };
    ui_panel(xpBox, COL_SURFACE, COL_AMBER, 2);
    ui_text("REWARD", xpBox.x + 16, xpBox.y + 12, 12, COL_AMBER, FontKind::MonoBold);
    std::string gained = b.xpGained > 0 ? "+" + std::to_string(b.xpGained) + " XP" : "No new XP: beat your best stars";
    ui_text(gained, xpBox.x + 16, xpBox.y + 34, 24, COL_AMBER, FontKind::MonoBold);
    ui_level_badge(xpBox.x + 16, xpBox.y + 74, progress_xp(app.progress));
    if (b.firstWin && b.region + 1 < (int)app.campaign.size())
        ui_text_wrapped(app.campaign[b.region + 1].name + " is now open.", xpBox.x + 16, xpBox.y + 122, xpBox.width - 32, 15, COL_GREEN, FontKind::UiBold);

    // Mal, and the buttons
    float fy = SCREEN_H - 80;
    ui_text("Mal:", MARGIN, fy + 14, 15, COL_AMBER, FontKind::UiBold);
    ui_text_wrapped(reg.win, MARGIN + 40, fy + 14, 760, 15, COL_TEXT_SOFT);
    if (ui_button("Back to the map", { SCREEN_W - MARGIN - 190, fy, 190, 48 }, ButtonStyle::Primary, COL_GREEN)) { back_to_campaign(app); return; }
    if (ui_button("Fight again", { SCREEN_W - MARGIN - 190 - 12 - 150, fy, 150, 48 }, ButtonStyle::Outline, COL_TEXT)) open_boss(app, b.region);
}

static void lost_frame(App &app)
{
    BossState &b = app.boss;
    Workspace &w = app.work;
    const Region &reg = region(app);
    if (IsKeyPressed(KEY_ENTER)) { open_boss(app, b.region); return; }
    if (IsKeyPressed(KEY_ESCAPE)) { back_to_campaign(app); return; }

    mal_draw(app.mal, { 230, 320 }, 150);
    float bob = sinf((float)GetTime() * 2) * 4;
    draw_boss(reg.bossLook, { SCREEN_W - 250.0f, 220 + bob }, 150, 0, false);
    std::string winner = reg.bossName + " wins this round";
    ui_text(winner, SCREEN_W - 250.0f - ui_text_width(winner, 16, FontKind::UiBold) / 2, 320, 16, COL_CORAL, FontKind::UiBold);

    float x = 420, y = 110;
    ui_text("DEFEATED", x, y, 14, COL_CORAL, FontKind::MonoBold);
    ui_text("Mal was free()'d.", x, y + 24, 40, COL_TEXT, FontKind::UiBold);
    ui_text(b.lostReason, x, y + 80, 16, COL_TEXT_SOFT);
    std::string got = "You got " + reg.bossName + " down to " + std::to_string(boss_hp(b.totalTests, b.bestPassed)) + " HP (" +
                      std::to_string(b.bestPassed) + " of " + std::to_string(b.totalTests) + " tests).";
    ui_text(got, x, y + 108, 16, COL_TEXT_SOFT);

    // Where it got you: the first test that still failed
    Rectangle box = { x, y + 150, 440, 150 };
    ui_panel(box, COL_SURFACE, COL_CORAL, 1);
    ui_text("WHERE IT GOT YOU", box.x + 16, box.y + 12, 12, COL_CORAL, FontKind::MonoBold);
    std::string where = "Your last run didn't get as far as the tests.";
    for (const TestResult &t : w.result.tests) if (!t.passed) { where = t.description; break; }
    if (w.result.status == RunStatus::CompileError) where = "Your last run didn't compile.";
    if (w.result.status == RunStatus::Crashed) where += " (and then it crashed)";
    ui_text_wrapped(where, box.x + 16, box.y + 36, box.width - 32, 15, COL_TEXT);
    ui_text_wrapped("Try the lesson and quiz for this region again, or have another go.", box.x + 16, box.y + 100, box.width - 32, 13, COL_MUTED);

    float fy = SCREEN_H - 80;
    if (ui_button("Try again", { SCREEN_W - MARGIN - 160, fy, 160, 48 }, ButtonStyle::Primary, COL_CORAL)) { open_boss(app, b.region); return; }
    if (ui_button("Back to the map", { SCREEN_W - MARGIN - 160 - 12 - 180, fy, 180, 48 }, ButtonStyle::Outline, COL_TEXT)) back_to_campaign(app);
    ui_text("ENTER try again   ESC back to the map", MARGIN, fy + 16, 13, COL_MUTED, FontKind::Mono);
}

void screen_boss(App &app)
{
    if (app.boss.region < 0 || app.boss.region >= (int)app.campaign.size()) { app.screen = Screen::Campaign; return; }
    switch (app.boss.phase) {
        case BOSS_INTRO: intro_frame(app); break;
        case BOSS_FIGHT: fight_frame(app); break;
        case BOSS_WON:   won_frame(app); break;
        default:         lost_frame(app); break;
    }
}
