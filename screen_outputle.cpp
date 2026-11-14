// screen_outputle.cpp - Outputle: today's C snippet, six tries to type exactly
// what it prints, Wordle colours, and bytes for solving it. The rules are in
// outputle.cpp.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include "editor.h"
#include <cmath>

static const Color TILE_GREEN = { 46, 160, 67, 255 };
static const Color TILE_YELLOW = { 200, 150, 40, 255 };
static const Color TILE_GREY = { 58, 64, 72, 255 };

static std::string typed;          // the row being typed
static double revealAt = -100;     // when the last guess was entered (its tiles flip over one by one)
static double shakeAt = -100;      // a guess that wasn't allowed shakes the row
static std::string notice;         // "It's 4 characters long", "Copied!"
static double noticeAt = -100;
static int bytesJustWon = 0;
static bool celebrate = false;    // set when it's solved: the next frame throws confetti

void open_outputle(App &app)
{
    if (app.screen != Screen::Outputle) app.returnTo = app.screen;
    typed.clear();
    revealAt = -100;
    app.screen = Screen::Outputle;
}

static Color tile_colour(int c)
{
    return c == 2 ? TILE_GREEN : c == 1 ? TILE_YELLOW : TILE_GREY;
}

static void say(const std::string &text)
{
    notice = text;
    noticeAt = GetTime();
}

static void submit(App &app, const std::string &today)
{
    int won = 0;
    std::string why;
    if (!outputle_guess(app.progress, today, typed, won, why)) {
        shakeAt = GetTime();
        sfx_play(Sfx::Wrong);
        say(why);
        return;
    }
    typed.clear();
    revealAt = GetTime();
    save_progress(app);
    if (outputle_won(app.progress, today)) {
        bytesJustWon = won;
        celebrate = true;
        learned_today(app);
        quest_event(app, "outputle");
        sfx_play(Sfx::LevelUp);
        mal_react(app.mal, MalMood::Cheer, 3.5f);
    } else if (outputle_over(app.progress, today)) {
        sfx_play(Sfx::Crumble);
        mal_react(app.mal, MalMood::Sad, 2.5f);
    } else {
        sfx_play(Sfx::Click);
    }
}

// One tile: a character, coloured once revealed. flip (0..1) squashes it as it turns over.
static void draw_tile(Rectangle r, char ch, int colour, float flip, bool typedTile)
{
    float squash = fabsf(cosf(flip * PI));   // 1 -> 0 -> 1 as it turns
    bool showColour = colour >= 0 && flip >= 0.5f;
    Rectangle s = { r.x, r.y + r.height * (1 - squash) / 2, r.width, fmaxf(2, r.height * squash) };
    if (showColour) DrawRectangleRounded(s, 0.12f, 6, tile_colour(colour));
    else {
        DrawRectangleRounded(s, 0.12f, 6, COL_BG);
        DrawRectangleRoundedLinesEx(s, 0.12f, 6, 2, typedTile ? COL_MUTED : COL_LINE);
    }
    if (ch && squash > 0.3f) {
        std::string t(1, ch);
        float w = ui_text_width(t, 30, FontKind::MonoBold);
        ui_text(t, r.x + (r.width - w) / 2, r.y + (r.height - 30) / 2 - 2, 30, COL_TEXT, FontKind::MonoBold);
    }
}

void screen_outputle(App &app)
{
    float t = (float)GetTime();
    std::string today = today_string();
    const OutPuzzle &puzzle = outputle_puzzles()[outputle_index(today)];
    const std::vector<std::string> &guesses = outputle_guesses(app.progress, today);
    bool over = outputle_over(app.progress, today), won = outputle_won(app.progress, today);
    int len = (int)puzzle.answer.size();

    // ----- Typing -----
    if (IsKeyPressed(KEY_ESCAPE)) { app.screen = app.returnTo; return; }
    if (!over) {
        for (int c = GetCharPressed(); c > 0; c = GetCharPressed())
            if (c > 32 && c < 127 && c != '|' && (int)typed.size() < len) typed += (char)c;
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !typed.empty()) typed.pop_back();
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) submit(app, today);
    }

    // ----- Title -----
    if (ui_button("Back", { SCREEN_W - MARGIN - 110, 22, 110, 40 }, ButtonStyle::Outline, COL_TEXT)) { app.screen = app.returnTo; return; }
    std::string title = "OUTPUTLE  #" + std::to_string(outputle_number(today));
    float tw = ui_text_width(title, 34, FontKind::MonoBold);
    ui_text(title, (SCREEN_W - tw) / 2, 28, 34, COL_TEXT, FontKind::MonoBold);
    const char *sub = "one snippet a day. guess exactly what it prints. six tries.";
    ui_text(sub, (SCREEN_W - ui_text_width(sub, 14, FontKind::Mono)) / 2, 74, 14, COL_MUTED, FontKind::Mono);

    // ----- The snippet -----
    Rectangle code = { 80, 118, 520, 56.0f + 30 * puzzle.code.size() };
    ui_panel(code, COL_SURFACE, COL_LINE, 1);
    for (int i = 0; i < (int)puzzle.code.size(); i++)
        editor_draw_code_line(puzzle.code[i], code.x + 24, code.y + 22 + i * 30, 18, 0, 46);
    std::string topicName = puzzle.topic;
    for (const Topic &tp : app.topics) if (tp.id == puzzle.topic) topicName = tp.title;
    ui_text("today's topic: " + topicName, code.x + 24, code.y + code.height - 28, 13, COL_MUTED, FontKind::Mono);

    // ----- The result, once it's over -----
    float ry = code.y + code.height + 18;
    Rectangle res = { 80, ry, 520, SCREEN_H - FOOTER_H - 16 - ry };
    if (over) {
        Color edge = won ? COL_GREEN : COL_CORAL;
        ui_panel(res, won ? Color{ 16, 38, 24, 255 } : Color{ 40, 20, 22, 255 }, edge, 1);
        std::string head = won ? (guesses.size() == 1 ? "First try!" : "Solved in " + std::to_string(guesses.size()) + "!")
                               : "It printed " + puzzle.answer;
        ui_text(head, res.x + 24, res.y + 20, 24, edge, FontKind::MonoBold);
        float h = ui_text_wrapped(puzzle.why, res.x + 24, res.y + 60, res.width - 150, 15, COL_TEXT_SOFT, FontKind::Mono);
        if (won) {
            ui_text("+" + std::to_string(outputle_reward((int)guesses.size())) + " bytes", res.x + 24, res.y + 72 + h, 15, COL_PURPLE,
                    FontKind::MonoBold);
        }
        ui_text("A new one at midnight.", res.x + 24, res.y + res.height - 30, 13, COL_MUTED, FontKind::Mono);
        mal_set_base(app.mal, won ? MalMood::Happy : MalMood::Idle);
        mal_draw_with_pet(app.mal, { res.x + res.width - 70, res.y + res.height - 18 }, 70);
    } else {
        ui_panel(res, COL_SURFACE, COL_LINE_SOFT, 1);
        ui_text("HOW TO PLAY", res.x + 24, res.y + 18, 12, COL_AMBER, FontKind::MonoBold);
        ui_text_wrapped("Type exactly what the code prints (" + std::to_string(len) + " characters, no spaces) and press ENTER.",
                        res.x + 24, res.y + 42, res.width - 48, 15, COL_TEXT_SOFT);
        float y = res.y + 96;
        const char *rows[] = { "right character, right place", "in the output, somewhere else", "not in the output" };
        for (int i = 0; i < 3; i++) {
            DrawRectangleRounded({ res.x + 24, y + i * 30, 22, 22 }, 0.2f, 4, tile_colour(2 - i));
            ui_text(rows[i], res.x + 58, y + i * 30 + 3, 14, COL_TEXT_SOFT);
        }
        mal_set_base(app.mal, MalMood::Idle);
        mal_draw_with_pet(app.mal, { res.x + res.width - 70, res.y + res.height - 18 }, 70);
    }

    // ----- The board -----
    const float cell = 62, gap = 8;
    float bw = len * cell + (len - 1) * gap;
    float bx = 820 - bw / 2, by = 118;
    for (int row = 0; row < OUTPUTLE_TRIES; row++) {
        float shake = (row == (int)guesses.size() && GetTime() - shakeAt < 0.4) ? sinf(t * 60) * 8 * (float)(1 - (GetTime() - shakeAt) / 0.4) : 0;
        std::vector<int> colours;
        std::string text;
        bool isTyping = false;
        if (row < (int)guesses.size()) { text = guesses[row]; colours = outputle_grade(text, puzzle.answer); }
        else if (row == (int)guesses.size() && !over) { text = typed; isTyping = true; }
        for (int c = 0; c < len; c++) {
            Rectangle r = { bx + c * (cell + gap) + shake, by + row * (cell + gap), cell, cell };
            char ch = c < (int)text.size() ? text[c] : 0;
            float flip = 1;
            if (!colours.empty() && row == (int)guesses.size() - 1) {
                flip = (float)((GetTime() - revealAt - c * 0.18) / 0.35);
                flip = flip < 0 ? 0 : flip > 1 ? 1 : flip;
            }
            draw_tile(r, ch, colours.empty() ? -1 : colours[c], colours.empty() ? 0 : flip, isTyping && ch);
        }
        if (isTyping && fmodf(t, 1.0f) < 0.6f && (int)typed.size() < len) {   // the cursor
            Rectangle r = { bx + typed.size() * (cell + gap), by + row * (cell + gap), cell, cell };
            DrawRectangle((int)(r.x + cell / 2 - 10), (int)(r.y + cell - 14), 20, 3, COL_AMBER);
        }
    }
    float belowBoard = by + OUTPUTLE_TRIES * (cell + gap) + 8;

    // A notice under the board, or the share button
    if (GetTime() - noticeAt < 2.2) {
        float nw = ui_text_width(notice, 15, FontKind::UiBold);
        ui_text(notice, 820 - nw / 2, belowBoard + 4, 15, notice == "Copied!" ? COL_GREEN : COL_CORAL, FontKind::UiBold);
    } else if (!over) {
        const char *hint = "type, BACKSPACE, ENTER";
        ui_text(hint, 820 - ui_text_width(hint, 13, FontKind::Mono) / 2, belowBoard + 6, 13, COL_MUTED, FontKind::Mono);
    }
    if (over) {
        Rectangle share = { 820 - 150, belowBoard + 34, 300, 46 };
        if (ui_button("Share (copies the squares)", share, ButtonStyle::Primary, COL_AMBER)) {
            SetClipboardText(outputle_share(app.progress, today).c_str());
            say("Copied!");
            sfx_play(Sfx::Click);
        }
    }

    // ----- Stats, on the right -----
    const OutputleStats &s = app.progress.outputleStats;
    float sx = 1050, sy = 122;
    ui_text(std::to_string(s.streak), sx, sy, 48, COL_AMBER, FontKind::MonoBold);
    ui_text("Outputle streak", sx, sy + 58, 14, COL_TEXT_SOFT);
    ui_text("best " + std::to_string(s.best), sx, sy + 80, 13, COL_MUTED, FontKind::Mono);
    sy += 124;
    ui_text(std::to_string(s.played) + " played", sx, sy, 14, COL_TEXT, FontKind::MonoBold);
    int pct = s.played > 0 ? s.won * 100 / s.played : 0;
    ui_text(std::to_string(pct) + "% solved", sx, sy + 22, 14, COL_TEXT, FontKind::MonoBold);
    sy += 62;
    ui_text("TRIES", sx, sy, 11, COL_MUTED, FontKind::MonoBold);
    int most = 1;
    for (int d : s.dist) if (d > most) most = d;
    for (int i = 0; i < 6; i++) {
        float y = sy + 20 + i * 26;
        ui_text(std::to_string(i + 1), sx, y + 2, 13, COL_TEXT_SOFT, FontKind::Mono);
        float w = 24 + 120.0f * s.dist[i] / most;
        bool mine = won && (int)guesses.size() == i + 1;
        DrawRectangleRounded({ sx + 18, y, w, 20 }, 0.2f, 4, mine ? TILE_GREEN : TILE_GREY);
        ui_text(std::to_string(s.dist[i]), sx + 18 + w - 16, y + 3, 12, COL_TEXT, FontKind::MonoBold);
    }

    if (celebrate) {   // just solved: confetti and the bytes
        celebrate = false;
        fx_burst({ 820, by + 3 * (cell + gap) }, 80, TILE_GREEN, COL_AMBER);
        fx_float_text("+" + std::to_string(bytesJustWon) + " bytes", { 820, by - 10 }, 26, COL_PURPLE);
    }
    ui_footer_hints({ "type what it prints", "BACKSPACE delete", "ENTER guess", "ESC back" });
}
