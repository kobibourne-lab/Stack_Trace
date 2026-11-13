// screen_watch.cpp - watching a Versus match as a spectator. Connects to the
// host on WATCH_PORT (so a spectator can never take the player's place), then:
//   predict: before the match, pick who'll win and put bytes on it
//   watch:   both players' code as they type, their test results, a live feed
//   result:  the winner, both solutions, and what everyone's prediction paid
// Bytes are play money, saved in progress.txt. What the messages mean, the
// pool and the payouts are in versus.cpp (tested); the host side is in
// screen_versus.cpp.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "sfx.h"
#include "fx.h"
#include <cmath>

static const Color SIDE_COL[2] = { COL_AMBER, COL_BLUE };   // the host, the joiner

static std::string two_digits(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }
static std::string clock_text(double seconds)
{
    if (seconds < 0) seconds = 0;
    int s = (int)seconds;
    return two_digits(s / 60) + ":" + two_digits(s % 60);
}

static Color tint(Color c, float a) { return ColorAlphaBlend(COL_SURFACE, Fade(c, a), WHITE); }

static std::string name_of(const App &app, int side)
{
    const std::string &n = app.watch.spec.side[side].them.name;
    return n.empty() ? (side == 0 ? "The host" : "Their opponent") : n;
}

static bool connected(const App &app) { return app.watch.net.state == NetState::Connected; }

static void save(App &app)
{
    std::string error;
    progress_save(app.progressPath, app.progress, error);
}

// Nobody gets stuck with nothing to predict with
static void top_up(App &app)
{
    if (app.progress.bytes < MIN_BYTES) { app.progress.bytes = MIN_BYTES; save(app); }
}

// The bytes of a prediction that didn't get a result come back.
static void refund(App &app)
{
    WatchState &w = app.watch;
    if (w.locked && !w.paid) {
        app.progress.bytes += w.lockedBytes;
        w.paid = true;
        w.payout = w.lockedBytes;
        save(app);
    }
}

static int total_tests(const App &app)
{
    const std::string &id = app.watch.spec.side[0].problemId;
    for (const ProblemSet &s : app.practice)
        for (const Problem &p : s.problems)
            if (p.id == id) return count_tests(p);
    return 0;
}

static const Problem *match_problem(const App &app)
{
    const std::string &id = app.watch.spec.side[0].problemId;
    for (const ProblemSet &s : app.practice)
        for (const Problem &p : s.problems)
            if (p.id == id) return &p;
    return nullptr;
}

static std::vector<std::string> split_code(const std::string &code)
{
    std::vector<std::string> lines;
    std::string line;
    for (char c : code) {
        if (c == '\n') { lines.push_back(line); line.clear(); }
        else if (c == '\t') line += "    ";
        else line += c;
    }
    lines.push_back(line);
    while (lines.size() > 1 && lines.back().empty()) lines.pop_back();
    return lines;
}

// Every frame: dress both players' Mals and the crowd from their LOOKs, and
// let them react: a hop for tests passing, a squash for a compile error, and
// the whole crowd cheers when someone gets more tests passing.
static void update_mals(App &app)
{
    WatchState &w = app.watch;
    VsSpectate &s = w.spec;
    float dt = GetFrameTime();
    bool cheer = false;
    for (int i = 0; i < 2; i++) {
        const VsPlayer &p = s.side[i].them;
        Mal &m = w.mals[i];
        mal_wear(m, p.look.hat, p.look.skin, p.look.pet);
        if (p.runs < w.runsSeen[i]) { w.runsSeen[i] = 0; w.bestSeen[i] = 0; }   // a new match
        if (p.runs > w.runsSeen[i]) {
            w.runsSeen[i] = p.runs;
            const std::string &r = p.lastResult;
            if (r == "compile" || r == "crash" || r == "timeout" || p.passed == 0) mal_react(m, MalMood::Sad, 1.4f);
            else mal_react(m, MalMood::Happy, 1.2f);
        }
        if (p.bestPassed > w.bestSeen[i]) { w.bestSeen[i] = p.bestPassed; cheer = true; }
        if (p.finished && !w.doneSeen[i]) { mal_react(m, MalMood::Cheer, 3.0f); cheer = true; }
        w.doneSeen[i] = p.finished;
        // After the match: the winner keeps cheering
        MalMood base = MalMood::Idle;
        if (s.phase == VsPhase::Finished && (s.winner == 0 || s.winner == 1)) base = s.winner == i ? MalMood::Cheer : MalMood::Sad;
        mal_set_base(m, base);
        mal_update(m, dt);
    }
    w.crowd.resize(s.room.size());
    for (size_t i = 0; i < s.room.size(); i++) {
        mal_wear(w.crowd[i], s.room[i].look.hat, s.room[i].look.skin, s.room[i].look.pet);
        if (cheer) mal_react(w.crowd[i], i % 2 ? MalMood::Happy : MalMood::Cheer, 1.0f + 0.15f * i);
        mal_update(w.crowd[i], dt);
    }
}

// ---------------------------------------------------------------------------

void open_watch(App &app, const std::string &address)
{
    WatchState &w = app.watch;
    net_close(w.net);
    w = WatchState();
    w.address = address;
    net_join(w.net, address, WATCH_PORT, GetTime());
    top_up(app);
    app.screen = Screen::Watch;
}

void watch_quit(App &app)
{
    WatchState &w = app.watch;
    if (w.locked && !w.paid && w.spec.phase == VsPhase::Lobby) refund(app);   // the host drops it from the pool
    if (connected(app)) {
        net_send(w.net, "BYE");
        net_update(w.net, GetTime());
    }
    net_close(w.net);
}

static void stop_watching(App &app)
{
    watch_quit(app);
    open_versus(app);
}

static void update_network(App &app, double now)
{
    WatchState &w = app.watch;
    VsSpectate &s = w.spec;
    bool was = connected(app);
    net_update(w.net, now);
    if (connected(app) && !w.helloSent) {
        net_send(w.net, "WATCH " + std::to_string(VERSUS_VERSION) + " " + vs_clean_name(app.vs.myName));
        VsLook look;   // our Mal sits in the crowd
        look.hat = app.mal.hat;
        look.skin = app.mal.skin;
        look.pet = app.mal.pet;
        net_send(w.net, vs_look_line(look));
        w.helloSent = true;
    }
    VsPhase before = s.phase;
    std::string line;
    while (net_receive(w.net, line)) {
        vs_spectate_apply(s, line, now);
        for (int i = 0; i < 2; i++) if (!s.side[i].notice.empty()) { w.message = s.side[i].notice; s.side[i].notice.clear(); }
    }
    vs_spectate_tick(s, now);

    // A new match: a fresh prediction
    if (s.phase == VsPhase::Lobby && before != VsPhase::Lobby) {
        w.locked = false;
        w.paid = false;
        w.pick = -1;
        w.payout = 0;
        top_up(app);
    }
    if (s.phase == VsPhase::Countdown && before == VsPhase::Lobby) sfx_play(Sfx::Click);

    // The result: pay out once
    if (s.phase == VsPhase::Finished && s.winner != -2 && w.locked && !w.paid) {
        int pool[2];
        vs_pool(s.room, pool);
        w.payout = vs_payout(w.lockedBytes, w.lockedSide, s.winner, pool);
        app.progress.bytes += w.payout;
        w.paid = true;
        save(app);
        if (w.payout > w.lockedBytes) {
            sfx_play(Sfx::Combo);
            fx_burst({ SCREEN_W - 220.0f, 120 }, 50, COL_GREEN, COL_PURPLE);
        } else if (w.payout < w.lockedBytes) {
            sfx_play(Sfx::Wrong);
        }
    }

    // The host went
    if (was && w.net.state != NetState::Connected) {
        refund(app);
        w.message = "The host closed the match.";
    }
}

static void send_prediction(App &app)
{
    WatchState &w = app.watch;
    int amount = std::min(w.amount, app.progress.bytes);
    if (w.locked || w.pick < 0 || amount <= 0 || !w.spec.open || !connected(app)) return;
    net_send(w.net, "PREDICT " + std::to_string(w.pick) + " " + std::to_string(amount));
    app.progress.bytes -= amount;
    w.locked = true;
    w.lockedBytes = amount;
    w.lockedSide = w.pick;
    save(app);
    sfx_play(Sfx::Correct);
}

// The header, with this spectator's name and bytes on the right.
static void header(App &app, const std::string &title)
{
    ui_header(title);
    std::string bytes = std::to_string(app.progress.bytes) + " bytes";
    float bw = ui_text_width(bytes, 15, FontKind::MonoBold) + 28;
    Rectangle chip = { SCREEN_W - MARGIN - bw, 14, bw, 36 };
    ui_panel(chip, COL_BG, COL_PURPLE, 1);
    ui_text(bytes, chip.x + 14, chip.y + 9, 15, COL_PURPLE, FontKind::MonoBold);
    std::string who = "Watching as " + vs_clean_name(app.vs.myName);
    ui_text(who, chip.x - 16 - ui_text_width(who, 13), 25, 13, COL_MUTED);
}

// ---------------------------------------------------------------------------
// Not connected (yet)
// ---------------------------------------------------------------------------

static void draw_waiting(App &app)
{
    WatchState &w = app.watch;
    header(app, "Versus  >  Watch a match");
    Rectangle box = { SCREEN_W / 2.0f - 300, 220, 600, 200 };
    ui_panel(box, COL_SURFACE, w.net.state == NetState::Failed || w.spec.full ? COL_CORAL : COL_PURPLE, 1);
    std::string title, text;
    if (w.spec.full) {
        title = "This match is full";
        text = "It already has " + std::to_string(MAX_WATCHERS) + " spectators. Try again when someone leaves.";
    } else if (w.net.state == NetState::Failed) {
        title = "Couldn't watch";
        text = w.message.empty() ? w.net.error : w.message;
    } else {
        title = "Connecting to " + w.address + "...";
        text = "The host needs to have pressed Host a match. All of you need to be on the same phone hotspot.";
    }
    ui_text(title, box.x + 28, box.y + 28, 22, COL_TEXT, FontKind::UiBold);
    ui_text_wrapped(text, box.x + 28, box.y + 70, box.width - 56, 15, COL_TEXT_SOFT);
    if (ui_button("Back", { box.x + 28, box.y + box.height - 28 - 44, 140, 44 }, ButtonStyle::Outline, COL_TEXT)) stop_watching(app);
    ui_footer_hints({ "ESC back" });
}

// ---------------------------------------------------------------------------
// Before the match: predict
// ---------------------------------------------------------------------------

static void draw_predict(App &app)
{
    WatchState &w = app.watch;
    VsSpectate &s = w.spec;
    bool both = !s.side[1].them.name.empty();
    header(app, "Versus  >  Watching " + name_of(app, 0) + (both ? " vs " + name_of(app, 1) : ""));

    int pool[2];
    vs_pool(s.room, pool);
    bool canPick = s.open && !w.locked && both && connected(app);

    // Keys
    if (canPick) {
        if (IsKeyPressed(KEY_ONE)) w.pick = 0;
        if (IsKeyPressed(KEY_TWO)) w.pick = 1;
    }

    float x = MARGIN, y = HEADER_H + 20;
    ui_text(w.locked ? "PREDICTION LOCKED IN" : s.open ? "PREDICTIONS ARE OPEN" : "PREDICTIONS ARE CLOSED", x, y, 12,
            w.locked ? COL_PURPLE : COL_AMBER, FontKind::MonoBold);
    ui_text("Who solves it first?", x, y + 18, 26, COL_TEXT, FontKind::UiBold);
    std::string closes = both ? "Closes when " + name_of(app, 0) + " starts the match" : "Waiting for a second player to join";
    ui_text(closes, SCREEN_W - MARGIN - 340 - 16 - ui_text_width(closes, 14), y + 28, 14, COL_TEXT_SOFT);

    // ----- The two players -----
    float leftW = SCREEN_W - 2 * MARGIN - 340 - 16;
    float cardW = (leftW - 16) / 2, cardH = 230;
    float cy = y + 64;
    for (int i = 0; i < 2; i++) {
        Rectangle r = { x + i * (cardW + 16), cy, cardW, cardH };
        bool here = !s.side[i].them.name.empty();
        bool picked = (w.locked ? w.lockedSide : w.pick) == i;
        bool hover = canPick && here && CheckCollisionPointRec(GetMousePosition(), r);
        if (hover) { ui_want_hand(); if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { w.pick = i; sfx_play(Sfx::Click); } }
        Color c = SIDE_COL[i];
        ui_panel(r, picked ? tint(c, 0.12f) : hover ? COL_SURFACE_2 : COL_SURFACE, picked ? c : COL_LINE, picked ? 2 : 1);
        if (!here) {
            ui_text("Waiting for someone to join...", r.x + 24, r.y + 30, 16, COL_MUTED);
            continue;
        }
        mal_badge(w.mals[i], { r.x + 48, r.y + 50 }, 26, c);
        ui_text(name_of(app, i), r.x + 88, r.y + 26, 24, COL_TEXT, FontKind::UiBold);
        std::string lvl = s.level[i] > 0 ? "Level " + std::to_string(s.level[i]) + "  |  " + level_title(s.level[i]) : "";
        ui_text(lvl, r.x + 88, r.y + 58, 13, c, FontKind::Mono);
        std::string key = std::to_string(i + 1);
        if (picked) ui_text(w.locked ? "LOCKED IN" : "YOUR PICK", r.x + r.width - 20 - ui_text_width(w.locked ? "LOCKED IN" : "YOUR PICK", 12, FontKind::MonoBold), r.y + 22, 12, c, FontKind::MonoBold);
        else if (canPick) ui_text("press " + key, r.x + r.width - 20 - ui_text_width("press " + key, 12, FontKind::Mono), r.y + 22, 12, COL_MUTED, FontKind::Mono);

        // Stats
        int wins = s.score[i], losses = s.score[1 - i];
        std::string stats[3][2] = {
            { std::to_string(wins) + " - " + std::to_string(losses), "vs " + name_of(app, 1 - i) },
            { std::to_string(s.solved[i]), "problems solved" },
            { std::to_string(s.score[2]), s.score[2] == 1 ? "draw" : "draws" } };
        float sw = (r.width - 40 - 16) / 3;
        for (int k = 0; k < 3; k++) {
            Rectangle b = { r.x + 20 + k * (sw + 8), r.y + 96, sw, 58 };
            ui_panel(b, COL_BG, COL_LINE_SOFT, 1);
            ui_text(stats[k][0], b.x + 12, b.y + 8, 18, COL_TEXT, FontKind::MonoBold);
            ui_text(stats[k][1], b.x + 12, b.y + 34, 12, COL_MUTED);
        }

        // What a prediction on them pays, with what's in the pool now
        int amount = std::min(w.amount, app.progress.bytes);
        int extra = (w.locked || amount <= 0) ? 0 : amount;
        float odds = (pool[i] + extra) > 0 ? (float)(pool[0] + pool[1] + extra) / (pool[i] + extra) : 0;
        char buf[48];
        std::snprintf(buf, sizeof buf, "%.2fx", odds);
        std::string pays = pool[0] + pool[1] + extra == 0 ? "No predictions yet" : std::string("Pays ") + buf + " what you put in";
        ui_text(pays, r.x + 20, r.y + 172, 14, COL_TEXT_SOFT);
        std::string backers;
        for (const VsWatcher &wt : s.room) if (wt.side == i) backers += (backers.empty() ? "" : ", ") + wt.name;
        if (!backers.empty()) {
            backers = "Backed by " + backers;
            ui_text(backers, r.x + r.width - 20 - ui_text_width(backers, 13), r.y + 196, 13, COL_MUTED);
        }
    }

    // ----- How much -----
    Rectangle amountBox = { x, cy + cardH + 16, leftW, 112 };
    ui_panel(amountBox, COL_SURFACE, COL_LINE, 1);
    if (w.locked) {
        ui_text("Locked in: " + std::to_string(w.lockedBytes) + " bytes on " + name_of(app, w.lockedSide) + ".",
                amountBox.x + 20, amountBox.y + 22, 18, COL_PURPLE, FontKind::UiBold);
        ui_text("You'll see both players' code as they type. Good luck!", amountBox.x + 20, amountBox.y + 56, 15, COL_TEXT_SOFT);
    } else if (!s.open) {
        ui_text("Predictions are closed for this match. Watch, and predict the next one.", amountBox.x + 20, amountBox.y + 22, 16, COL_TEXT_SOFT);
    } else {
        std::string q = w.pick < 0 ? "Pick a player, then how much" : "How much on " + name_of(app, w.pick) + "?";
        ui_text(q, amountBox.x + 20, amountBox.y + 16, 15, COL_TEXT, FontKind::UiBold);
        int options[4] = { 10, 25, 50, app.progress.bytes };
        const char *labels[4] = { "10", "25", "50", "All" };
        int sel = -1;
        for (int k = 0; k < 4; k++) if (w.amount == options[k]) sel = k;
        if (IsKeyPressed(KEY_RIGHT)) { sel = sel < 0 ? 0 : std::min(3, sel + 1); w.amount = options[sel]; }
        if (IsKeyPressed(KEY_LEFT)) { sel = sel < 0 ? 0 : std::max(0, sel - 1); w.amount = options[sel]; }
        float bx = amountBox.x + 20, bw = 96;
        for (int k = 0; k < 4; k++) {
            bool affordable = options[k] <= app.progress.bytes && options[k] > 0;
            Rectangle b = { bx + k * (bw + 10), amountBox.y + 48, bw, 44 };
            bool on = sel == k;
            bool hover = affordable && CheckCollisionPointRec(GetMousePosition(), b);
            if (hover) { ui_want_hand(); if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) w.amount = options[k]; }
            ui_panel(b, on ? tint(COL_PURPLE, 0.2f) : hover ? COL_SURFACE_2 : COL_BG, on ? COL_PURPLE : COL_LINE, on ? 2 : 1);
            ui_text(labels[k], b.x + (bw - ui_text_width(labels[k], 16, FontKind::MonoBold)) / 2, b.y + 12, 16,
                    !affordable ? COL_MUTED : on ? COL_PURPLE : COL_TEXT, FontKind::MonoBold);
        }
        int amount = std::min(w.amount, app.progress.bytes);
        std::string lock = w.pick < 0 ? "Pick a player first" : "Lock in " + std::to_string(amount) + " on " + name_of(app, w.pick);
        Rectangle lb = { bx + 4 * (bw + 10) + 6, amountBox.y + 48, amountBox.x + amountBox.width - 20 - (bx + 4 * (bw + 10) + 6), 44 };
        if (ui_button(lock, lb, canPick && w.pick >= 0 ? ButtonStyle::Primary : ButtonStyle::Disabled, COL_AMBER) ||
            (canPick && w.pick >= 0 && IsKeyPressed(KEY_ENTER)))
            send_prediction(app);
        if (w.pick >= 0) {
            int p2[2] = { pool[0], pool[1] };
            p2[w.pick] += amount;
            std::string back = "If " + name_of(app, w.pick) + " wins you get " + std::to_string(vs_payout(amount, w.pick, w.pick, p2)) + " back";
            ui_text(back, amountBox.x + amountBox.width - 20 - ui_text_width(back, 13), amountBox.y + 18, 13, COL_GREEN);
        }
    }

    // ----- Right: the match, the pool, the room -----
    float rx = x + leftW + 16, rw = 340, ry = HEADER_H + 20;
    Rectangle matchBox = { rx, ry, rw, 132 };
    ui_panel(matchBox, COL_SURFACE, COL_LINE, 1);
    ui_text("THE MATCH", matchBox.x + 18, matchBox.y + 14, 12, COL_BLUE, FontKind::MonoBold);
    const VsMatch &m0 = s.side[0];
    std::string setTitle = m0.lobbySet;
    for (const ProblemSet &ps : app.practice) if (ps.id == m0.lobbySet) setTitle = ps.title;
    ui_text(setTitle.empty() ? "Waiting for the host's settings" : setTitle, matchBox.x + 18, matchBox.y + 38, 18, COL_TEXT, FontKind::UiBold);
    std::string detail = (m0.lobbyDifficulty == "Any" ? "Any difficulty" : m0.lobbyDifficulty) + "  |  " + std::to_string(m0.lobbyMinutes) + " minutes";
    ui_text(detail, matchBox.x + 18, matchBox.y + 66, 14, COL_TEXT_SOFT, FontKind::Mono);
    ui_text_wrapped("The host picks a problem at random when they press Start.", matchBox.x + 18, matchBox.y + 92, rw - 36, 12, COL_MUTED);

    Rectangle poolBox = { rx, ry + 132 + 14, rw, 132 };
    ui_panel(poolBox, COL_SURFACE, COL_LINE, 1);
    int total = pool[0] + pool[1];
    ui_text("THE POOL  |  " + std::to_string(total) + " BYTES", poolBox.x + 18, poolBox.y + 14, 12, COL_PURPLE, FontKind::MonoBold);
    Rectangle bar = { poolBox.x + 18, poolBox.y + 42, rw - 36, 14 };
    DrawRectangleRounded(bar, 1, 8, COL_LINE_SOFT);
    if (total > 0) {
        float split = bar.width * pool[0] / total;
        if (pool[0] > 0) DrawRectangleRounded({ bar.x, bar.y, split, bar.height }, 1, 8, SIDE_COL[0]);
        if (pool[1] > 0) DrawRectangleRounded({ bar.x + split, bar.y, bar.width - split, bar.height }, 1, 8, SIDE_COL[1]);
    }
    std::string l0 = name_of(app, 0) + " " + std::to_string(pool[0]), l1 = name_of(app, 1) + " " + std::to_string(pool[1]);
    ui_text(l0, bar.x, bar.y + 22, 13, SIDE_COL[0], FontKind::Mono);
    ui_text(l1, bar.x + bar.width - ui_text_width(l1, 13, FontKind::Mono), bar.y + 22, 13, SIDE_COL[1], FontKind::Mono);
    ui_text_wrapped("Winners split the whole pool, so the less popular pick pays more.", poolBox.x + 18, poolBox.y + 88, rw - 36, 12, COL_MUTED);

    float roomY = poolBox.y + poolBox.height + 14;
    Rectangle roomBox = { rx, roomY, rw, SCREEN_H - FOOTER_H - 12 - roomY };
    ui_panel(roomBox, COL_SURFACE, COL_LINE, 1);
    ui_text("IN THE ROOM", roomBox.x + 18, roomBox.y + 14, 12, COL_MUTED, FontKind::MonoBold);
    float ly = roomBox.y + 40;
    for (int i = 0; i < 2; i++) {
        if (s.side[i].them.name.empty()) continue;
        mal_draw(w.mals[i], { roomBox.x + 26, ly + 19 }, 17);
        ui_text(name_of(app, i), roomBox.x + 44, ly, 14, COL_TEXT);
        ui_text("playing", roomBox.x + rw - 18 - ui_text_width("playing", 13), ly + 1, 13, COL_MUTED);
        ly += 28;
    }
    for (size_t k = 0; k < s.room.size(); k++) {
        const VsWatcher &wt = s.room[k];
        if (k < w.crowd.size()) mal_draw(w.crowd[k], { roomBox.x + 26, ly + 19 }, 17);
        bool me = wt.name == vs_clean_name(app.vs.myName);
        ui_text(wt.name + (me ? " (you)" : ""), roomBox.x + 44, ly, 14, COL_TEXT);
        std::string what = wt.side < 0 ? "picking..." : std::to_string(wt.bytes) + " on " + name_of(app, wt.side);
        ui_text(what, roomBox.x + rw - 18 - ui_text_width(what, 13), ly + 1, 13, wt.side < 0 ? COL_MUTED : COL_PURPLE);
        ly += 28;
    }

    if (!w.message.empty()) ui_text(w.message, x, SCREEN_H - FOOTER_H - 30, 14, COL_CORAL);
    ui_footer_hints({ "1 / 2 pick a player", "LEFT / RIGHT amount", "ENTER lock in", "ESC stop watching" });
}

// ---------------------------------------------------------------------------
// During the match: watch
// ---------------------------------------------------------------------------

static void draw_bar(float x, float y, float w, int filled, int total, Color c, bool fromRight)
{
    int n = total > 0 ? total : 1;
    float gap = 4, seg = (w - gap * (n - 1)) / n;
    for (int i = 0; i < n; i++) {
        int k = fromRight ? n - 1 - i : i;
        Rectangle r = { x + k * (seg + gap), y, seg, 14 };
        if (i < filled) DrawRectangleRounded(r, 0.3f, 4, c);
        else { DrawRectangleRounded(r, 0.3f, 4, COL_LINE_SOFT); DrawRectangleRoundedLinesEx(r, 0.3f, 4, 1, COL_LINE); }
    }
}

static void draw_watch(App &app)
{
    WatchState &w = app.watch;
    VsSpectate &s = w.spec;
    double now = GetTime();
    int total = total_tests(app);
    for (int i = 0; i < 2; i++) if (s.side[i].them.total > total) total = s.side[i].them.total;

    // ----- The top: both players' progress, and the clock -----
    float hudH = 92;
    DrawRectangle(0, 0, SCREEN_W, (int)hudH, { 17, 22, 29, 255 });
    DrawLine(0, (int)hudH - 1, SCREEN_W, (int)hudH - 1, COL_LINE_SOFT);
    float half = (SCREEN_W - 2 * 32 - 240) / 2.0f;
    for (int i = 0; i < 2; i++) {
        const VsPlayer &p = s.side[i].them;
        float bx = i == 0 ? 32 : SCREEN_W - 32 - half;
        int best = p.finished ? total : p.bestPassed;
        std::string nm = name_of(app, i), tests = std::to_string(best) + " / " + std::to_string(total) + " tests";
        if (p.finished) tests = "SOLVED " + clock_text(p.finishSeconds);
        if (i == 0) {
            mal_badge(w.mals[0], { bx + 15, 31 }, 15, SIDE_COL[0]);
            ui_text(nm, bx + 38, 22, 17, COL_TEXT, FontKind::UiBold);
            ui_text(tests, bx + half - ui_text_width(tests, 13, FontKind::Mono), 25, 13, SIDE_COL[i], FontKind::Mono);
        } else {
            ui_text(tests, bx, 25, 13, SIDE_COL[i], FontKind::Mono);
            mal_badge(w.mals[1], { bx + half - 15, 31 }, 15, SIDE_COL[1]);
            ui_text(nm, bx + half - 38 - ui_text_width(nm, 17, FontKind::UiBold), 22, 17, COL_TEXT, FontKind::UiBold);
        }
        draw_bar(bx, 50, half, best, total, SIDE_COL[i], i == 1);
    }
    std::string watching = "WATCHING  |  " + std::to_string(s.room.size()) + (s.room.size() == 1 ? " SPECTATOR" : " SPECTATORS");
    ui_text(watching, SCREEN_W / 2.0f - ui_text_width(watching, 11, FontKind::Mono) / 2, 12, 11, COL_PURPLE, FontKind::Mono);
    double left = s.side[0].minutes * 60.0 - vs_elapsed(s.side[0], now);
    if (s.phase == VsPhase::Countdown) left = s.side[0].minutes * 60.0;
    std::string t = clock_text(left);
    float tw = ui_text_width(t, 26, FontKind::MonoBold) + 36;
    Color tc = left < 60 ? COL_CORAL : COL_AMBER;
    ui_panel({ SCREEN_W / 2.0f - tw / 2, 32, tw, 44 }, COL_BG, tc, 1);
    ui_text(t, SCREEN_W / 2.0f - tw / 2 + 18, 41, 26, tc, FontKind::MonoBold);

    // ----- Both players' code, as they type -----
    float top = hudH + 14, bottom = SCREEN_H - FOOTER_H - 10;
    float rw = 290, gap = 14;
    float pw = (SCREEN_W - 2 * 32 - rw - 2 * gap) / 2;
    for (int i = 0; i < 2; i++) {
        const VsPlayer &p = s.side[i].them;
        Rectangle pane = { 32 + i * (pw + gap), top, pw, bottom - top - 76 };
        ui_panel(pane, COL_SURFACE, SIDE_COL[i], 1);
        ui_text(name_of(app, i) + "'s code", pane.x + 14, pane.y + 10, 13, SIDE_COL[i], FontKind::MonoBold);
        std::string state = p.left ? "left" : p.finished ? "solved" : p.running ? "running..." : "typing...";
        ui_text(state, pane.x + pane.width - 14 - ui_text_width(state, 12, FontKind::Mono), pane.y + 11, 12, COL_MUTED, FontKind::Mono);
        DrawLine((int)pane.x, (int)(pane.y + 34), (int)(pane.x + pane.width), (int)(pane.y + 34), COL_LINE_SOFT);
        Rectangle codeR = { pane.x + 4, pane.y + 38, pane.width - 8, pane.height - 42 };
        if (s.live[i].empty()) ui_text("Waiting for their code...", codeR.x + 12, codeR.y + 10, 13, COL_MUTED);
        else {
            // Long code: show the end, where they're typing
            std::vector<std::string> lines = split_code(s.live[i]);
            int fits = (int)(codeR.height / 21);
            if ((int)lines.size() > fits && fits > 0) lines.erase(lines.begin(), lines.end() - fits);
            ui_code(lines, codeR, 0, SIDE_COL[i], false, 13);
        }

        Rectangle res = { pane.x, pane.y + pane.height + 10, pw, 66 };
        ui_panel(res, COL_SURFACE, COL_LINE, 1);
        std::string last = p.lastResult.empty() ? "No runs yet" : "Last run: " + vs_result_text(p.lastResult, p.passed, p.total);
        Color lc = p.lastResult.empty() ? COL_MUTED : p.finished ? COL_GREEN
                 : (p.lastResult == "compile" || p.lastResult == "crash" || p.lastResult == "timeout") ? COL_CORAL : SIDE_COL[i];
        if (p.finished) last = "Accepted in " + clock_text(p.finishSeconds);
        ui_text(last, res.x + 14, res.y + 12, 15, lc, FontKind::UiBold);
        std::string runs = std::to_string(p.runs) + (p.runs == 1 ? " run" : " runs");
        ui_text(runs, res.x + res.width - 14 - ui_text_width(runs, 12, FontKind::Mono), res.y + 14, 12, COL_MUTED, FontKind::Mono);
        std::string sub = p.compileErrors > 0 ? std::to_string(p.compileErrors) + (p.compileErrors == 1 ? " compile error so far" : " compile errors so far") : "";
        ui_text(sub, res.x + 14, res.y + 40, 12, COL_TEXT_SOFT);
    }

    // ----- Right: your prediction, and the feed -----
    float rx = SCREEN_W - 32 - rw;
    Rectangle pred = { rx, top, rw, 132 };
    ui_panel(pred, COL_SURFACE, COL_PURPLE, 1);
    ui_text("YOUR PREDICTION", pred.x + 16, pred.y + 14, 12, COL_PURPLE, FontKind::MonoBold);
    int pool[2];
    vs_pool(s.room, pool);
    if (w.locked) {
        ui_text(std::to_string(w.lockedBytes) + " bytes on " + name_of(app, w.lockedSide), pred.x + 16, pred.y + 38, 16, COL_TEXT, FontKind::UiBold);
        int back = vs_payout(w.lockedBytes, w.lockedSide, w.lockedSide, pool);
        ui_text("If " + name_of(app, w.lockedSide) + " wins: +" + std::to_string(back) + " back", pred.x + 16, pred.y + 64, 13, COL_GREEN);
    } else {
        ui_text("None this match", pred.x + 16, pred.y + 38, 16, COL_TEXT_SOFT, FontKind::UiBold);
        ui_text("Predict the next one in the lobby.", pred.x + 16, pred.y + 64, 13, COL_MUTED);
    }
    int ptotal = pool[0] + pool[1];
    Rectangle bar = { pred.x + 16, pred.y + 92, rw - 32, 10 };
    DrawRectangleRounded(bar, 1, 8, COL_LINE_SOFT);
    if (ptotal > 0) {
        float split = bar.width * pool[0] / ptotal;
        if (pool[0] > 0) DrawRectangleRounded({ bar.x, bar.y, split, bar.height }, 1, 8, SIDE_COL[0]);
        if (pool[1] > 0) DrawRectangleRounded({ bar.x + split, bar.y, bar.width - split, bar.height }, 1, 8, SIDE_COL[1]);
    }
    ui_text("Pool: " + std::to_string(ptotal) + " bytes, locked", pred.x + 16, pred.y + 108, 12, COL_MUTED);

    Rectangle feed = { rx, top + 132 + 12, rw, bottom - (top + 132 + 12) };
    ui_panel(feed, COL_SURFACE, COL_LINE, 1);
    ui_text("LIVE FEED", feed.x + 16, feed.y + 14, 12, COL_BLUE, FontKind::MonoBold);
    float fy = feed.y + 40;
    float standsH = w.crowd.empty() ? 0 : 92;   // the crowd at the bottom
    for (const VsNamedEvent &e : vs_spectate_feed(s)) {
        float h = ui_text_wrapped_height(e.text, rw - 80, 13) + 8;
        if (fy + h > feed.y + feed.height - 8 - standsH) break;
        ui_text(clock_text(e.seconds), feed.x + 16, fy + 1, 11, COL_MUTED, FontKind::Mono);
        Color c = e.kind == 3 ? COL_GREEN : e.kind == 1 ? COL_CORAL : SIDE_COL[e.side];
        DrawCircleV({ feed.x + 62, fy + 8 }, 4, c);
        ui_text_wrapped(e.text, feed.x + 74, fy, rw - 84, 13, COL_TEXT_SOFT);
        fy += h;
    }
    if (!w.crowd.empty()) {   // the stands: every spectator's Mal, cheering when someone passes more tests
        float sy = feed.y + feed.height - standsH;
        DrawLine((int)feed.x + 12, (int)sy, (int)(feed.x + feed.width - 12), (int)sy, COL_LINE_SOFT);
        ui_text("IN THE STANDS", feed.x + 16, sy + 10, 11, COL_PURPLE, FontKind::MonoBold);
        float step = fminf(64.0f, (rw - 32) / (float)w.crowd.size());
        for (size_t k = 0; k < w.crowd.size(); k++) {
            float cx = feed.x + 16 + step * (k + 0.5f);
            mal_draw(w.crowd[k], { cx, sy + 78 }, 34);
            std::string nm = s.room[k].name;
            if (nm.size() > 7) nm = nm.substr(0, 6) + ".";
            ui_text(nm, cx - ui_text_width(nm, 10, FontKind::Mono) / 2, sy + 80, 10, COL_MUTED, FontKind::Mono);
        }
    }

    // The countdown, over everything
    if (s.phase == VsPhase::Countdown) {
        DrawRectangle(0, (int)hudH, SCREEN_W, SCREEN_H - (int)hudH, Fade(COL_BG, 0.82f));
        double c = s.side[0].startAt - now;
        std::string n = std::to_string((int)std::ceil(c > 0 ? c : 0.01));
        ui_text(n, SCREEN_W / 2.0f - ui_text_width(n, 200, FontKind::MonoBold) / 2, 200, 200, COL_AMBER, FontKind::MonoBold);
        const Problem *p = match_problem(app);
        std::string sub = p ? p->title + "  |  " + p->difficulty + "  |  " + std::to_string(s.side[0].minutes) + " minutes" : "Get ready";
        ui_text(sub, SCREEN_W / 2.0f - ui_text_width(sub, 20) / 2, 440, 20, COL_TEXT_SOFT);
    }

    ui_footer_hints({ "Watching: " + std::to_string(s.room.size()) + " in the room", "ESC stop watching",
                      "The players can't see each other's code. You can." });
}

// ---------------------------------------------------------------------------
// After the match: the result and the payouts
// ---------------------------------------------------------------------------

static void draw_result(App &app)
{
    WatchState &w = app.watch;
    VsSpectate &s = w.spec;
    int win = s.winner;
    header(app, "Versus  >  Result");

    float x = MARGIN, y = HEADER_H + 22;
    ui_text("MATCH OVER", x, y, 12, COL_AMBER, FontKind::MonoBold);
    std::string title = (win == 0 || win == 1) ? name_of(app, win) + " wins!" : win == -1 ? "It's a draw" : "Match over";
    ui_text(title, x, y + 18, 38, COL_TEXT, FontKind::UiBold);
    if (win == 0 || win == 1)   // the winner's Mal, cheering beside it
        mal_draw(w.mals[win], { x + ui_text_width(title, 38, FontKind::UiBold) + 46, y + 84 }, 44);
    std::string why;
    if (win == 0 || win == 1) {
        const VsPlayer &p = s.side[win].them, &o = s.side[1 - win].them;
        if (p.finished) why = "Solved it in " + clock_text(p.finishSeconds) + ".";
        else if (o.left) why = name_of(app, 1 - win) + " left the match.";
        else why = "Nobody solved it, but " + name_of(app, win) + " passed more tests.";
        if (!o.finished && !o.left && p.finished) why += " " + name_of(app, 1 - win) + " had " + std::to_string(o.bestPassed) + " tests passing.";
    } else if (win == -1) {
        why = "Everyone gets their bytes back.";
    }
    ui_text(why, x, y + 66, 15, COL_TEXT_SOFT);

    // Your payout
    Rectangle mine = { SCREEN_W - MARGIN - 330, y, 330, 92 };
    if (w.locked) {
        int delta = w.payout - w.lockedBytes;
        bool right = delta > 0;
        Color c = delta > 0 ? COL_GREEN : delta < 0 ? COL_CORAL : COL_TEXT_SOFT;
        ui_panel(mine, COL_SURFACE, c, 2);
        ui_text(right ? "YOUR PREDICTION WAS RIGHT" : delta < 0 ? "NOT THIS TIME" : "YOUR BYTES CAME BACK", mine.x + 16, mine.y + 12, 12, c, FontKind::MonoBold);
        std::string d = (delta > 0 ? "+" : "") + std::to_string(delta) + " bytes";
        ui_text(d, mine.x + 16, mine.y + 30, 28, c, FontKind::MonoBold);
        ui_text("Put in " + std::to_string(w.lockedBytes) + ", got " + std::to_string(w.payout) + " back. Now " +
                std::to_string(app.progress.bytes) + ".", mine.x + 16, mine.y + 66, 13, COL_TEXT_SOFT);
    } else {
        ui_panel(mine, COL_SURFACE, COL_LINE, 1);
        ui_text("NO PREDICTION", mine.x + 16, mine.y + 12, 12, COL_MUTED, FontKind::MonoBold);
        ui_text_wrapped("You didn't predict this one. Stay for the rematch and pick before it starts.", mine.x + 16, mine.y + 34, 298, 14, COL_TEXT_SOFT);
    }

    // Both solutions, the winner first
    y += 110;
    float rw = 300, gap = 14;
    float pw = (SCREEN_W - 2 * MARGIN - rw - 2 * gap) / 2;
    float h = SCREEN_H - FOOTER_H - 74 - y;
    int order[2] = { win == 1 ? 1 : 0, win == 1 ? 0 : 1 };
    for (int k = 0; k < 2; k++) {
        int i = order[k];
        const VsPlayer &p = s.side[i].them;
        Rectangle pane = { MARGIN + k * (pw + gap), y, pw, h };
        bool winner = i == win;
        ui_panel(pane, COL_SURFACE, winner ? SIDE_COL[i] : COL_LINE, winner ? 2 : 1);
        ui_text(name_of(app, i) + (winner ? "  |  winner" : ""), pane.x + 14, pane.y + 11, 14, SIDE_COL[i], FontKind::UiBold);
        std::string stats = (p.finished ? clock_text(p.finishSeconds) : p.left ? "left" : "time up") + "  |  " +
                            std::to_string(p.runs) + " runs  |  " + std::to_string(p.finished ? p.total : p.bestPassed) + " / " +
                            std::to_string(p.total);
        ui_text(stats, pane.x + pane.width - 14 - ui_text_width(stats, 12, FontKind::Mono), pane.y + 13, 12, COL_MUTED, FontKind::Mono);
        DrawLine((int)pane.x, (int)(pane.y + 36), (int)(pane.x + pane.width), (int)(pane.y + 36), COL_LINE_SOFT);
        std::string code = p.hasCode ? p.code : s.live[i];
        Rectangle codeR = { pane.x + 4, pane.y + 40, pane.width - 8, pane.height - 44 };
        if (code.empty()) ui_text("Their code didn't arrive.", codeR.x + 12, codeR.y + 10, 13, COL_MUTED);
        else ui_code(split_code(code), codeR, 0, SIDE_COL[i], false, 13);
    }

    // Everyone's payouts
    float rx = SCREEN_W - MARGIN - rw;
    int pool[2];
    vs_pool(s.room, pool);
    Rectangle pays = { rx, y, rw, h };
    ui_panel(pays, COL_SURFACE, COL_PURPLE, 1);
    ui_text("PAYOUTS  |  POOL " + std::to_string(pool[0] + pool[1]), pays.x + 16, pays.y + 14, 12, COL_PURPLE, FontKind::MonoBold);
    float py = pays.y + 42;
    for (const VsWatcher &wt : s.room) {
        if (wt.side < 0) continue;
        int back = vs_payout(wt.bytes, wt.side, win, pool);
        int delta = back - wt.bytes;
        ui_text(wt.name, pays.x + 16, py, 16, COL_TEXT, FontKind::UiBold);
        std::string d = (delta > 0 ? "+" : "") + std::to_string(delta);
        Color c = delta > 0 ? COL_GREEN : delta < 0 ? COL_CORAL : COL_TEXT_SOFT;
        ui_text(d, pays.x + rw - 16 - ui_text_width(d, 16, FontKind::MonoBold), py, 16, c, FontKind::MonoBold);
        ui_text(std::to_string(wt.bytes) + " on " + name_of(app, wt.side) + ", got " + std::to_string(back) + " back",
                pays.x + 16, py + 22, 12, COL_MUTED);
        py += 52;
    }
    if (py == pays.y + 42) ui_text("Nobody predicted this match.", pays.x + 16, py, 13, COL_MUTED);

    // Head-to-head, and the buttons
    float fy = SCREEN_H - FOOTER_H - 58;
    std::string score = "Head to head: " + name_of(app, 0) + " " + std::to_string(s.score[0]) + " - " + std::to_string(s.score[1]) + " " + name_of(app, 1);
    ui_text(score, MARGIN, fy + 14, 15, COL_TEXT_SOFT, FontKind::Mono);
    if (ui_button("Stop watching", { SCREEN_W - MARGIN - 170, fy, 170, 46 }, ButtonStyle::Outline, COL_TEXT)) { stop_watching(app); return; }
    ui_text("Stay here for the rematch", SCREEN_W - MARGIN - 170 - 20 - ui_text_width("Stay here for the rematch", 14), fy + 15, 14, COL_PURPLE);
    ui_footer_hints({ "ESC stop watching", "the next match's predictions open when they rematch" });
}

// ---------------------------------------------------------------------------

void screen_watch(App &app)
{
    WatchState &w = app.watch;
    update_network(app, GetTime());
    update_mals(app);
    if (IsKeyPressed(KEY_ESCAPE)) { stop_watching(app); return; }

    bool ready = connected(app) && !w.spec.side[0].them.name.empty();
    if (!ready || w.spec.full) { draw_waiting(app); return; }
    switch (w.spec.phase) {
        case VsPhase::Lobby:     draw_predict(app); break;
        case VsPhase::Countdown:
        case VsPhase::Playing:   draw_watch(app); break;
        case VsPhase::Finished:  draw_result(app); break;
    }
}
