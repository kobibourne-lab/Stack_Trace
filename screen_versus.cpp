// screen_versus.cpp - Versus mode: two people, one problem, the same clock.
//
//   Lobby      host or join, pick names and the match settings
//   Countdown  3, 2, 1 (the problem stays hidden)
//   Playing    the problem screen's editor and results, plus a scoreboard and
//              a feed of what the other player is doing (never their code)
//   Finished   who won, both players' stats, and both solutions side by side
//
// net.cpp moves the lines between the two computers; versus.cpp decides what
// they mean. This file draws it all and handles the keys.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include <cmath>

static Color tint(Color c, float amount)
{
    return { (unsigned char)(COL_BG.r + (c.r - COL_BG.r) * amount), (unsigned char)(COL_BG.g + (c.g - COL_BG.g) * amount),
             (unsigned char)(COL_BG.b + (c.b - COL_BG.b) * amount), 255 };
}

static std::string two_digits(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

static std::string clock_text(double seconds)
{
    if (seconds < 0) seconds = 0;
    int s = (int)std::ceil(seconds - 0.001);
    return two_digits(s / 60) + ":" + two_digits(s % 60);
}

static std::string initials(const std::string &name)
{
    std::string s;
    for (char c : name) if (isalpha((unsigned char)c) && s.size() < 2) s += (char)toupper((unsigned char)c);
    return s.empty() ? "?" : s;
}

static Color difficulty_color(const std::string &d)
{
    if (d == "Easy") return COL_GREEN;
    if (d == "Hard") return COL_CORAL;
    if (d == "Medium") return COL_AMBER;
    return COL_TEXT_SOFT;
}

static void send(App &app, const std::string &line) { net_send(app.vs.net, line); }

static bool connected(const App &app) { return app.vs.net.state == NetState::Connected; }

// Finds a problem by its id ("exam2/09_sum_array"). Returns false if this copy doesn't have it.
static bool find_problem(const App &app, const std::string &id, int &set, int &problem)
{
    for (int s = 0; s < (int)app.practice.size(); s++)
        for (int p = 0; p < (int)app.practice[s].problems.size(); p++)
            if (app.practice[s].problems[p].id == id) { set = s; problem = p; return true; }
    return false;
}

static int count_lines(const EditBuffer &b)
{
    int n = 0;
    for (const std::string &line : b.lines)
        if (line.find_first_not_of(' ') != std::string::npos) n++;
    return n;
}

// ---------------------------------------------------------------------------
// Opening and leaving
// ---------------------------------------------------------------------------

void open_versus(App &app)
{
    VersusState &vs = app.vs;
    if (vs.myName.empty()) vs.myName = vs_default_name();
    vs.localAddress = net_local_address();
    // The two laptops share a hotspot, so the host's address starts the same as ours
    if (vs.joinAddress.empty() && !vs.localAddress.empty()) {
        size_t dot = vs.localAddress.rfind('.');
        if (dot != std::string::npos) vs.joinAddress = vs.localAddress.substr(0, dot + 1);
    }
    if (vs.match.lobbySet.empty() && !app.practice.empty()) vs.match.lobbySet = app.practice[0].id;
    vs.message.clear();
    app.screen = Screen::Versus;
}

// Back to the lobby screen with no connection.
static void disconnect(App &app)
{
    VersusState &vs = app.vs;
    net_close(vs.net);
    int wins = vs.match.myWins, losses = vs.match.theirWins, draws = vs.match.draws;
    std::string set = vs.match.lobbySet, diff = vs.match.lobbyDifficulty;
    int minutes = vs.match.lobbyMinutes;
    vs.match = VsMatch();
    vs.match.myWins = wins;
    vs.match.theirWins = losses;
    vs.match.draws = draws;
    vs.match.lobbySet = set;
    vs.match.lobbyDifficulty = diff;
    vs.match.lobbyMinutes = minutes;
    vs.helloSent = false;
    vs.focus = 0;
}

void versus_quit(App &app)
{
    VersusState &vs = app.vs;
    if (!connected(app)) return;
    if (vs.match.phase == VsPhase::Playing || vs.match.phase == VsPhase::Countdown) send(app, "LEFT");
    send(app, "BYE");
    net_update(vs.net, GetTime());   // one last chance to send them
    net_close(vs.net);
}

static void leave_versus(App &app)
{
    versus_quit(app);
    disconnect(app);
    open_exam_prep(app);
}

// Opens the match's problem in the workspace, set up for Versus.
static bool start_workspace(App &app)
{
    VsMatch &m = app.vs.match;
    int set = 0, problem = 0;
    if (!find_problem(app, m.problemId, set, problem)) return false;
    open_problem(app, set, problem, true);   // exam mode: fresh code, no hints, no saved draft
    app.screen = Screen::Versus;
    Workspace &w = app.work;
    w.versus = true;
    w.started = m.startAt;
    w.endsAt = m.startAt + m.minutes * 60.0;
    VersusState &vs = app.vs;
    vs.lastRunCount = 0;
    vs.lastLines = -1;
    vs.lastLinesSent = 0;
    vs.timeUpHandled = false;
    vs.codeSent = false;
    return true;
}

// The host picks a problem at random from the chosen set and difficulty.
static void host_start(App &app)
{
    VsMatch &m = app.vs.match;
    const ProblemSet *set = nullptr;
    for (const ProblemSet &s : app.practice) if (s.id == m.lobbySet) set = &s;
    if (set == nullptr && !app.practice.empty()) set = &app.practice[0];
    if (set == nullptr) return;

    std::vector<int> choices;
    for (int i = 0; i < (int)set->problems.size(); i++)
        if (m.lobbyDifficulty == "Any" || set->problems[i].difficulty == m.lobbyDifficulty)
            if (set->problems[i].id != m.problemId || set->problems.size() == 1) choices.push_back(i);
    if (choices.empty()) for (int i = 0; i < (int)set->problems.size(); i++) choices.push_back(i);
    const Problem &p = set->problems[choices[GetRandomValue(0, (int)choices.size() - 1)]];

    send(app, "START " + set->id + " " + p.id + " " + std::to_string(m.lobbyMinutes));
    vs_begin(m, set->id, p.id, m.lobbyMinutes, GetTime());
    start_workspace(app);
}

static void send_settings(App &app)
{
    VsMatch &m = app.vs.match;
    if (m.host && connected(app))
        send(app, "SETTINGS " + m.lobbySet + " " + m.lobbyDifficulty + " " + std::to_string(m.lobbyMinutes));
}

// ---------------------------------------------------------------------------
// The network, every frame
// ---------------------------------------------------------------------------

static void update_network(App &app, double now)
{
    VersusState &vs = app.vs;
    VsMatch &m = vs.match;
    bool wasConnected = connected(app);
    net_update(vs.net, now);

    if (connected(app) && !vs.helloSent) {
        send(app, "HELLO " + std::to_string(VERSUS_VERSION) + " " + vs.myName);
        vs.helloSent = true;
        send_settings(app);
    }

    std::string line;
    while (net_receive(vs.net, line)) {
        vs_apply(m, line, now);
        if (m.startRequested) {
            m.startRequested = false;
            if (!start_workspace(app)) {
                send(app, "MISSING " + m.problemId);
                m.phase = VsPhase::Lobby;
                vs.message = "Your copy doesn't have the problem " + m.them.name + " picked. Both pull the latest main and rebuild.";
            }
        }
        if (!m.notice.empty()) { vs.message = m.notice; m.notice.clear(); }
    }

    // The connection dropped
    if (wasConnected && vs.net.state == NetState::Failed) {
        std::string why = vs.net.error;
        if (m.phase == VsPhase::Playing || m.phase == VsPhase::Countdown) {
            m.them.left = true;
            m.them.over = true;
            m.feed.push_back({ vs_elapsed(m, now), "Disconnected", 1 });
            if (m.phase == VsPhase::Countdown) m.phase = VsPhase::Playing;
        } else if (m.phase == VsPhase::Lobby) {
            disconnect(app);
            vs.net.state = NetState::Failed;
            vs.net.error = why;
        }
        vs.helloSent = false;
    }
}

// ---------------------------------------------------------------------------
// Small pieces
// ---------------------------------------------------------------------------

// A one-line text box. Returns true when Enter is pressed in it.
static bool text_field(App &app, int id, std::string &value, Rectangle r, bool enabled, bool addressOnly, int maxLen)
{
    VersusState &vs = app.vs;
    bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), r);
    if (hover) ui_want_ibeam();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hover) vs.focus = id;
        else if (vs.focus == id) vs.focus = 0;
    }
    bool focused = enabled && vs.focus == id;
    bool enter = false;
    if (focused) {
        int c;
        while ((c = GetCharPressed()) > 0) {
            bool ok = addressOnly ? ((c >= '0' && c <= '9') || c == '.') : (isalnum(c) || c == '_' || c == '-');
            if (ok && (int)value.size() < maxLen) value += (char)c;
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !value.empty()) value.pop_back();
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) enter = true;
    }
    ui_panel(r, COL_BG, focused ? COL_BLUE : COL_LINE, 1);
    float size = addressOnly ? 20 : 17;
    Color c = enabled ? COL_TEXT : COL_MUTED;
    ui_text(value, r.x + 14, r.y + (r.height - size) / 2 - 2, size, c, addressOnly ? FontKind::MonoBold : FontKind::UiBold);
    if (focused && std::fmod(GetTime(), 1.0) < 0.55) {
        float cx = r.x + 14 + ui_text_width(value, size, addressOnly ? FontKind::MonoBold : FontKind::UiBold) + 2;
        DrawRectangle((int)cx, (int)(r.y + 10), 2, (int)r.height - 20, COL_AMBER);
    }
    return enter;
}

// A row of choices like "Easy  Medium  Hard". Returns the clicked one, or -1.
static int chips(const std::vector<std::string> &labels, int selected, float x, float y, bool enabled, Color accent)
{
    int clicked = -1;
    for (int i = 0; i < (int)labels.size(); i++) {
        float w = ui_text_width(labels[i], 14, FontKind::UiBold) + 28;
        Rectangle r = { x, y, w, 32 };
        bool on = (i == selected);
        bool hover = enabled && !on && CheckCollisionPointRec(GetMousePosition(), r);
        if (hover) { ui_want_hand(); if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i; }
        ui_panel(r, on ? tint(accent, 0.22f) : (hover ? COL_SURFACE_2 : COL_BG), on ? accent : COL_LINE, 1);
        ui_text(labels[i], x + 14, y + 7, 14, on ? accent : (enabled ? COL_TEXT_SOFT : COL_MUTED), FontKind::UiBold);
        x += w + 8;
    }
    return clicked;
}

static void avatar(const std::string &name, float cx, float cy, float r, Color c)
{
    DrawCircleV({ cx, cy }, r, c);
    DrawCircleV({ cx, cy }, r - 3, COL_BG);
    std::string i = initials(name);
    float size = r * 0.62f;
    ui_text(i, cx - ui_text_width(i, size, FontKind::MonoBold) / 2, cy - size * 0.62f, size, c, FontKind::MonoBold);
}

// ---------------------------------------------------------------------------
// Lobby
// ---------------------------------------------------------------------------

static void draw_lobby(App &app)
{
    VersusState &vs = app.vs;
    VsMatch &m = vs.match;
    NetConn &net = vs.net;
    double now = GetTime();
    bool idle = net.state == NetState::Idle || net.state == NetState::Failed;

    ui_header("Versus  >  " + std::string(connected(app) ? "Lobby" : "New match"));
    if (ui_button("Back to Exam prep", { SCREEN_W - MARGIN - 190, 12, 190, 40 }, ButtonStyle::Outline, COL_TEXT)) { leave_versus(app); return; }
    if (IsKeyPressed(KEY_ESCAPE) && vs.focus == 0) { leave_versus(app); return; }

    float x = MARGIN, y = HEADER_H + 24;
    float leftW = 820;
    ui_text("Code against a classmate", x, y, 28, COL_TEXT, FontKind::UiBold);
    y += 42;
    ui_text_wrapped("Same problem, same clock, exam-mode rules. You see how far they've got, never their code, until the end.",
                    x, y, 640, 15, COL_TEXT_SOFT);
    y += 44;

    // Your name
    ui_text("Your name", x, y + 9, 14, COL_MUTED);
    text_field(app, 1, vs.myName, { x + 96, y, 260, 38 }, idle, false, 16);
    if (vs.myName.empty() && vs.focus != 1) vs.myName = vs_default_name();
    y += 56;

    // ----- Host and Join cards -----
    float cardW = (leftW - 16) / 2, cardH = 196;
    Rectangle hostCard = { x, y, cardW, cardH }, joinCard = { x + cardW + 16, y, cardW, cardH };
    bool hosting = m.host && net.state != NetState::Idle && net.state != NetState::Failed;
    bool joining = !m.host && net.state != NetState::Idle && net.state != NetState::Failed;

    ui_panel(hostCard, COL_SURFACE, hosting ? COL_GREEN : COL_LINE, 1);
    ui_text("HOST", hostCard.x + 20, hostCard.y + 18, 12, hosting ? COL_GREEN : COL_MUTED, FontKind::MonoBold);
    if (hosting) {
        ui_text("Tell your opponent this address:", hostCard.x + 20, hostCard.y + 44, 14, COL_TEXT_SOFT);
        std::string addr = vs.localAddress.empty() ? "unknown" : vs.localAddress;
        ui_panel({ hostCard.x + 20, hostCard.y + 70, cardW - 40, 46 }, COL_BG, COL_LINE, 1);
        ui_text(addr, hostCard.x + 34, hostCard.y + 81, 22, COL_TEXT, FontKind::MonoBold);
        if (vs.localAddress.empty())
            ui_text("Find it with ipconfig getifaddr en0 (Mac) or ipconfig (Windows)", hostCard.x + 20, hostCard.y + 124, 12, COL_AMBER);
        if (net.state == NetState::Listening) {
            float pulse = 0.5f + 0.5f * std::sin((float)now * 4);
            DrawCircleV({ hostCard.x + 26, hostCard.y + 168 }, 5, Fade(COL_AMBER, 0.4f + 0.6f * pulse));
            ui_text("Waiting for someone to join...", hostCard.x + 40, hostCard.y + 159, 14, COL_AMBER);
            if (ui_button("Cancel", { hostCard.x + cardW - 20 - 90, hostCard.y + 150, 90, 34 }, ButtonStyle::Outline, COL_TEXT_SOFT)) disconnect(app);
        } else if (connected(app)) {
            DrawCircleV({ hostCard.x + 26, hostCard.y + 168 }, 5, COL_GREEN);
            std::string who = m.helloReceived ? m.them.name + " joined" : "Someone joined...";
            ui_text(who, hostCard.x + 40, hostCard.y + 159, 14, COL_GREEN, FontKind::UiBold);
            ui_text(net.peerAddress, hostCard.x + 48 + ui_text_width(who, 14, FontKind::UiBold), hostCard.y + 161, 12, COL_MUTED, FontKind::Mono);
        }
    } else {
        ui_text_wrapped("Start a match on this computer. Your opponent joins with your address.",
                        hostCard.x + 20, hostCard.y + 44, cardW - 40, 14, COL_TEXT_SOFT);
        if (ui_button("Host a match", { hostCard.x + 20, hostCard.y + cardH - 20 - 44, cardW - 40, 44 },
                      idle ? ButtonStyle::Primary : ButtonStyle::Disabled)) {
            disconnect(app);
            m.host = true;
            vs.localAddress = net_local_address();
            net_host(net, VERSUS_PORT);
        }
    }

    ui_panel(joinCard, COL_SURFACE, joining ? COL_BLUE : COL_LINE, 1);
    ui_text("OR JOIN", joinCard.x + 20, joinCard.y + 18, 12, joining ? COL_BLUE : COL_MUTED, FontKind::MonoBold);
    ui_text("The host's address", joinCard.x + 20, joinCard.y + 44, 14, COL_TEXT_SOFT);
    bool enter = text_field(app, 2, vs.joinAddress, { joinCard.x + 20, joinCard.y + 70, cardW - 40, 46 }, idle, true, 15);
    if (net.state == NetState::Connecting) {
        ui_text("Connecting to " + net.peerAddress + "...", joinCard.x + 20, joinCard.y + 159, 14, COL_AMBER);
        if (ui_button("Cancel", { joinCard.x + cardW - 20 - 90, joinCard.y + 150, 90, 34 }, ButtonStyle::Outline, COL_TEXT_SOFT)) disconnect(app);
    } else if (joining && connected(app)) {
        DrawCircleV({ joinCard.x + 26, joinCard.y + 168 }, 5, COL_GREEN);
        std::string who = "Joined " + (m.helloReceived ? m.them.name : net.peerAddress);
        ui_text(who, joinCard.x + 40, joinCard.y + 159, 14, COL_GREEN, FontKind::UiBold);
    } else if ((ui_button("Join match", { joinCard.x + 20, joinCard.y + cardH - 20 - 44, cardW - 40, 44 },
                          idle ? ButtonStyle::Outline : ButtonStyle::Disabled, COL_BLUE) || enter) && idle) {
        disconnect(app);
        m.host = false;
        vs.focus = 0;
        net_join(net, vs.joinAddress, VERSUS_PORT, now);
    }
    y += cardH + 16;

    // ----- Match settings (the host picks; the joiner sees them) -----
    Rectangle settings = { x, y, leftW, 128 };
    ui_panel(settings, COL_SURFACE, COL_LINE, 1);
    ui_text(m.host || !connected(app) ? "MATCH SETTINGS" : "MATCH SETTINGS  (" + m.them.name + " picks)",
            settings.x + 20, settings.y + 16, 12, COL_MUTED, FontKind::MonoBold);
    bool canEdit = m.host || !connected(app);

    std::vector<std::string> setLabels;
    int setSel = 0;
    for (int i = 0; i < (int)app.practice.size(); i++) {
        setLabels.push_back(app.practice[i].title);
        if (app.practice[i].id == m.lobbySet) setSel = i;
    }
    ui_text("Problems", settings.x + 20, settings.y + 50, 13, COL_MUTED);
    int c = chips(setLabels, setSel, settings.x + 20, settings.y + 72, canEdit, COL_PURPLE);
    if (c >= 0) { m.lobbySet = app.practice[c].id; send_settings(app); }

    std::vector<std::string> diffs = { "Any", "Easy", "Medium", "Hard" };
    int diffSel = 0;
    for (int i = 0; i < 4; i++) if (diffs[i] == m.lobbyDifficulty) diffSel = i;
    ui_text("Difficulty", settings.x + 280, settings.y + 50, 13, COL_MUTED);
    c = chips(diffs, diffSel, settings.x + 280, settings.y + 72, canEdit, difficulty_color(m.lobbyDifficulty == "Any" ? "Medium" : m.lobbyDifficulty));
    if (c >= 0) { m.lobbyDifficulty = diffs[c]; send_settings(app); }

    std::vector<std::string> mins = { "5", "10", "15", "20" };
    int minSel = 2;
    for (int i = 0; i < 4; i++) if (std::to_string(m.lobbyMinutes) == mins[i]) minSel = i;
    ui_text("Minutes", settings.x + 580, settings.y + 50, 13, COL_MUTED);
    c = chips(mins, minSel, settings.x + 580, settings.y + 72, canEdit, COL_AMBER);
    if (c >= 0) { m.lobbyMinutes = std::stoi(mins[c]); send_settings(app); }

    // ----- Right: the players, and Start -----
    float rx = x + leftW + 24, rw = SCREEN_W - MARGIN - rx;
    Rectangle players = { rx, HEADER_H + 24, rw, 300 };
    ui_panel(players, COL_SURFACE, COL_LINE, 1);
    ui_text("PLAYERS", players.x + 20, players.y + 18, 12, COL_MUTED, FontKind::MonoBold);
    float cy = players.y + 104;
    float leftCx = players.x + rw * 0.25f, rightCx = players.x + rw * 0.75f;
    avatar(vs.myName, leftCx, cy, 36, COL_GREEN);
    ui_text_centered_wrapped(vs.myName, leftCx, cy + 48, rw / 2 - 10, 15, COL_TEXT, 1, FontKind::UiBold);
    ui_text_centered_wrapped("you", leftCx, cy + 70, rw / 2, 12, COL_GREEN, 1, FontKind::Mono);
    ui_text("VS", players.x + rw / 2 - ui_text_width("VS", 20, FontKind::MonoBold) / 2, cy - 12, 20, COL_MUTED, FontKind::MonoBold);
    if (m.helloReceived && connected(app)) {
        avatar(m.them.name, rightCx, cy, 36, COL_BLUE);
        ui_text_centered_wrapped(m.them.name, rightCx, cy + 48, rw / 2 - 10, 15, COL_TEXT, 1, FontKind::UiBold);
        ui_text_centered_wrapped("ready", rightCx, cy + 70, rw / 2, 12, COL_BLUE, 1, FontKind::Mono);
    } else {
        DrawCircleLinesV({ rightCx, cy }, 36, COL_LINE);
        ui_text("?", rightCx - ui_text_width("?", 22, FontKind::MonoBold) / 2, cy - 14, 22, COL_LINE, FontKind::MonoBold);
        ui_text_centered_wrapped("waiting", rightCx, cy + 48, rw / 2, 13, COL_MUTED, 1);
    }

    Rectangle startBtn = { players.x + 20, players.y + players.height - 20 - 50, rw - 40, 50 };
    bool ready = connected(app) && m.helloReceived;
    if (m.host) {
        if (ui_button(ready ? "Start: 3, 2, 1..." : "Waiting for an opponent", startBtn, ready ? ButtonStyle::Primary : ButtonStyle::Disabled)
            || (ready && vs.focus == 0 && IsKeyPressed(KEY_ENTER))) {
            host_start(app);
            return;
        }
    } else if (ready) {
        ui_panel(startBtn, COL_BG, COL_LINE, 1);
        std::string t = "Waiting for " + m.them.name + " to start";
        ui_text(t, startBtn.x + (startBtn.width - ui_text_width(t, 15, FontKind::UiBold)) / 2, startBtn.y + 15, 15, COL_TEXT_SOFT, FontKind::UiBold);
    } else {
        ui_button("Host or join to start", startBtn, ButtonStyle::Disabled);
    }

    // Head-to-head, and messages
    float ny = players.y + players.height + 16;
    if (m.myWins + m.theirWins + m.draws > 0) {
        std::string score = "This session: " + vs.myName + " " + std::to_string(m.myWins) + " - " + std::to_string(m.theirWins) + " " +
                            (m.them.name.empty() ? "Opponent" : m.them.name);
        if (m.draws > 0) score += "  (" + std::to_string(m.draws) + (m.draws == 1 ? " draw)" : " draws)");
        ui_text(score, rx, ny, 13, COL_TEXT_SOFT, FontKind::Mono);
        ny += 28;
    }
    std::string problem = net.state == NetState::Failed ? net.error : vs.message;
    if (!problem.empty()) {
        float h = ui_text_wrapped_height(problem, rw - 36, 14) + 28;
        ui_panel({ rx, ny, rw, h }, COL_SURFACE, COL_CORAL, 1);
        ui_text_wrapped(problem, rx + 18, ny + 14, rw - 36, 14, COL_CORAL);
        ny += h + 12;
    }
    if (ny < SCREEN_H - FOOTER_H - 110) {
        Rectangle tip = { rx, ny, rw, 92 };
        ui_panel(tip, COL_SURFACE, COL_LINE_SOFT, 1);
        ui_text("CAN'T CONNECT?", tip.x + 18, tip.y + 14, 12, COL_AMBER, FontKind::MonoBold);
        ui_text_wrapped("College Wi-Fi blocks laptops talking to each other. Put both on one phone's hotspot, "
                        "and allow Stack Trace if a firewall asks.", tip.x + 18, tip.y + 36, rw - 36, 13, COL_TEXT_SOFT);
    }

    ui_footer_hints({ "click a box to type in it", "ENTER join / start", "ESC back" });
}

// ---------------------------------------------------------------------------
// Countdown
// ---------------------------------------------------------------------------

static void draw_countdown(App &app)
{
    VsMatch &m = app.vs.match;
    double left = m.startAt - GetTime();
    ui_header("Versus  >  Get ready");
    const Problem &p = app.practice[app.work.set].problems[app.work.problem];
    std::string sub = "A " + p.difficulty + " problem from " + app.practice[app.work.set].title + ", " + std::to_string(m.minutes) + " minutes";
    std::string n = std::to_string((int)std::ceil(left > 0 ? left : 0.01));
    float size = 220;
    ui_text(n, SCREEN_W / 2.0f - ui_text_width(n, size, FontKind::MonoBold) / 2, 180, size, COL_AMBER, FontKind::MonoBold);
    ui_text(sub, SCREEN_W / 2.0f - ui_text_width(sub, 20) / 2, 460, 20, COL_TEXT_SOFT);
    std::string vsLine = app.vs.myName + "  vs  " + m.them.name;
    ui_text(vsLine, SCREEN_W / 2.0f - ui_text_width(vsLine, 18, FontKind::MonoBold) / 2, 130, 18, COL_TEXT, FontKind::MonoBold);
}

// ---------------------------------------------------------------------------
// Playing
// ---------------------------------------------------------------------------

static void my_run(App &app, bool submit)
{
    if (app.work.runPhase != 0) return;
    problem_start_run(app, submit);
    app.vs.match.me.running = true;
    send(app, "RUNNING");
}

static void draw_scoreboard(App &app, float y)
{
    VsMatch &m = app.vs.match;
    float h = 64;
    DrawLine(0, (int)(y + h), SCREEN_W, (int)(y + h), COL_LINE_SOFT);

    auto side = [&](const VsPlayer &p, const std::string &name, bool left, Color c) {
        float w = (SCREEN_W - 2 * MARGIN - 80) / 2.0f;
        float x = left ? MARGIN : SCREEN_W - MARGIN - w;
        float ax = left ? x + 22 : x + w - 22;
        avatar(name, ax, y + h / 2, 21, c);
        float bx = left ? x + 56 : x;
        float bw = w - 56;
        std::string tests = p.total > 0 ? std::to_string(p.passed) + " / " + std::to_string(p.total) + " tests" : "no runs yet";
        if (p.finished) tests = "Solved in " + clock_text(p.finishSeconds);
        std::string label = left ? "You" : name;
        if (left) {
            ui_text(label, bx, y + 10, 15, COL_TEXT, FontKind::UiBold);
            ui_text(tests, bx + bw - ui_text_width(tests, 13, FontKind::Mono), y + 12, 13, c, FontKind::Mono);
        } else {
            ui_text(tests, bx, y + 12, 13, c, FontKind::Mono);
            ui_text(label, bx + bw - ui_text_width(label, 15, FontKind::UiBold), y + 10, 15, COL_TEXT, FontKind::UiBold);
        }
        Rectangle bar = { bx, y + 34, bw, 8 };
        DrawRectangleRounded(bar, 1, 4, COL_LINE_SOFT);
        float frac = p.finished ? 1.0f : (p.total > 0 ? (float)p.passed / p.total : 0);
        if (frac > 0) {
            float fw = bar.width * frac;
            DrawRectangleRounded({ left ? bar.x : bar.x + bar.width - fw, bar.y, fw, bar.height }, 1, 4, c);
        }
        std::string status;
        Color sc = COL_MUTED;
        if (p.left) { status = "Left the match"; sc = COL_CORAL; }
        else if (p.finished) { status = "Finished"; sc = COL_GREEN; }
        else if (p.running) { status = "Running code now..."; sc = COL_AMBER; }
        else if (!p.lastResult.empty()) {
            status = "Last run: " + vs_result_text(p.lastResult, p.passed, p.total);
            sc = p.lastResult == "compile" ? COL_CORAL : COL_TEXT_SOFT;
        } else status = left ? "Write your code, then Run" : "Writing code...";
        float sw = ui_text_width(status, 12);
        ui_text(status, left ? bx : bx + bw - sw, y + 47, 12, sc);
    };
    side(m.me, app.vs.myName, true, COL_GREEN);
    side(m.them, m.them.name, false, COL_BLUE);
    ui_text("VS", SCREEN_W / 2.0f - ui_text_width("VS", 16, FontKind::MonoBold) / 2, y + 24, 16, COL_MUTED, FontKind::MonoBold);
}

static void draw_feed(App &app, Rectangle r)
{
    VsMatch &m = app.vs.match;
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    std::string title = m.them.name;
    for (char &ch : title) ch = (char)toupper((unsigned char)ch);
    ui_text(title + "'S MATCH", r.x + 16, r.y + 16, 12, COL_BLUE, FontKind::MonoBold);

    float y = r.y + 44;
    int shown = 0;
    int maxShown = (int)((r.height - 44 - 130) / 26);
    int start = (int)m.feed.size() > maxShown ? (int)m.feed.size() - maxShown : 0;
    if (m.feed.empty()) { ui_text("Nothing yet", r.x + 16, y, 13, COL_MUTED); }
    for (int i = start; i < (int)m.feed.size(); i++, shown++) {
        const VsEvent &e = m.feed[i];
        Color c = e.kind == 1 ? COL_CORAL : e.kind == 2 ? COL_AMBER : e.kind == 3 ? COL_GREEN : COL_MUTED;
        ui_text(clock_text(e.seconds), r.x + 16, y + 1, 12, COL_MUTED, FontKind::Mono);
        DrawCircleV({ r.x + 72, y + 8 }, 4, c);
        ui_text(e.text, r.x + 84, y, 13, COL_TEXT_SOFT);
        y += 26;
    }
    if (m.them.running) {
        ui_text("Running code now...", r.x + 84, y, 13, COL_AMBER);
    }

    // Their code stays hidden: just its outline and length
    Rectangle hidden = { r.x + 12, r.y + r.height - 12 - 112, r.width - 24, 112 };
    ui_panel(hidden, COL_BG, COL_LINE, 1);
    ui_text("Their code: hidden until the end", hidden.x + 12, hidden.y + 10, 12, COL_MUTED);
    static const float widths[] = { 0.7f, 0.85f, 0.55f, 0.9f, 0.4f, 0.75f };
    static const float indents[] = { 0, 12, 12, 24, 12, 0 };
    int bars = m.them.lines < 6 ? m.them.lines : 6;
    for (int i = 0; i < bars; i++)
        DrawRectangleRounded({ hidden.x + 12 + indents[i], hidden.y + 34 + i * 9, (hidden.width - 60) * widths[i], 5 }, 1, 4, COL_LINE_SOFT);
    std::string lines = std::to_string(m.them.lines) + (m.them.lines == 1 ? " line" : " lines");
    ui_text(lines, hidden.x + 12, hidden.y + hidden.height - 22, 12, COL_TEXT_SOFT, FontKind::Mono);
}

static void play_frame(App &app)
{
    VersusState &vs = app.vs;
    VsMatch &m = vs.match;
    Workspace &w = app.work;
    double now = GetTime();

    problem_run_pending(app);

    // A run just finished: tell the other player
    if (w.runCount != vs.lastRunCount) {
        vs.lastRunCount = w.runCount;
        for (const std::string &line : vs_my_run(m, w.result, w.resultWasSubmit, now)) send(app, line);
        if (m.me.finished && !vs.codeSent) { send(app, "CODE " + vs_escape(eb_text(w.code))); vs.codeSent = true; }
    }

    // Time's up: lock the code and submit it, then say we're done
    if (!m.me.finished && !vs.timeUpHandled && vs_remaining(m, now) <= 0) {
        vs.timeUpHandled = true;
        w.timeUp = true;
        if (w.runPhase == 0) my_run(app, true);
    }
    if (vs.timeUpHandled && !m.me.over && w.runPhase == 0 && vs.lastRunCount == w.runCount) send(app, vs_my_time_up(m));

    // The length of my code, every couple of seconds if it changed
    int lines = count_lines(w.code);
    if (lines != vs.lastLines && now - vs.lastLinesSent > 2.0) {
        send(app, "LINES " + std::to_string(lines));
        vs.lastLines = lines;
        vs.lastLinesSent = now;
    }

    bool locked = w.timeUp || m.me.finished;
    bool cmd = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (!locked && ((cmd && IsKeyPressed(KEY_ENTER)) || IsKeyPressed(KEY_F5) || IsKeyPressed(KEY_F6)))
        my_run(app, shift || IsKeyPressed(KEY_F6));

    // ESC twice to leave (so one slip doesn't forfeit)
    static double escArmed = -10;
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (now - escArmed < 3.0) {
            send(app, "LEFT");
            m.me.left = true;
            m.me.over = true;
        } else {
            escArmed = now;
        }
    }

    // ----- Header: the shared clock in the middle -----
    const Problem &p = app.practice[w.set].problems[w.problem];
    ui_header("Versus  >  " + p.title);
    double left = vs_remaining(m, now);
    std::string t = clock_text(left);
    Color tc = left < 60 ? COL_CORAL : COL_AMBER;
    float tw = ui_text_width(t, 24, FontKind::MonoBold) + 36;
    ui_panel({ SCREEN_W / 2.0f - tw / 2, 11, tw, 42 }, COL_BG, tc, 1);
    ui_text(t, SCREEN_W / 2.0f - tw / 2 + 18, 20, 24, tc, FontKind::MonoBold);
    std::string conn = connected(app) ? "Connected to " + m.them.name : "Disconnected";
    float cw = ui_text_width(conn, 13);
    DrawCircleV({ SCREEN_W - MARGIN - cw - 14, 32 }, 4, connected(app) ? COL_GREEN : COL_CORAL);
    ui_text(conn, SCREEN_W - MARGIN - cw, 24, 13, COL_MUTED);

    draw_scoreboard(app, HEADER_H);

    // ----- Body: description | editor and results | their feed -----
    float top = HEADER_H + 64 + 14, bottom = SCREEN_H - FOOTER_H - 12;
    Rectangle desc = { MARGIN, top, 300, bottom - top };
    Rectangle feed = { SCREEN_W - MARGIN - 260, top, 260, bottom - top };
    float mx = desc.x + desc.width + 14, mw = feed.x - 14 - mx;
    Rectangle editorBox = { mx, top, mw, 276 };
    Rectangle console = { mx, editorBox.y + editorBox.height + 12, mw, bottom - (editorBox.y + editorBox.height + 12) };

    problem_draw_description(app, desc);
    problem_draw_editor(app, editorBox, locked);
    problem_draw_console(app, console);
    draw_feed(app, feed);

    // A banner over the editor once you've finished
    if (m.me.finished) {
        Rectangle b = { editorBox.x + 1, editorBox.y + editorBox.height - 44, editorBox.width - 2, 43 };
        DrawRectangleRec(b, tint(COL_GREEN, 0.2f));
        std::string msg = "You solved it in " + clock_text(m.me.finishSeconds) + ". Waiting for " + m.them.name + "...";
        ui_text(msg, b.x + 16, b.y + 13, 15, COL_GREEN, FontKind::UiBold);
    }

    // ----- Footer -----
    bool escShown = now - escArmed < 3.0;
    ui_footer_hints({ "Ctrl/Cmd+Enter run", "Ctrl/Cmd+Shift+Enter submit", escShown ? "ESC again to leave the match" : "ESC ESC leave" });
    float by = SCREEN_H - FOOTER_H + 8;
    ButtonStyle runStyle = (locked || w.runPhase != 0) ? ButtonStyle::Disabled : ButtonStyle::Outline;
    ButtonStyle submitStyle = (locked || w.runPhase != 0) ? ButtonStyle::Disabled : ButtonStyle::Primary;
    if (ui_button("Submit", { SCREEN_W - MARGIN - 120, by, 120, 40 }, submitStyle)) my_run(app, true);
    if (ui_button("Run", { SCREEN_W - MARGIN - 120 - 12 - 100, by, 100, 40 }, runStyle, COL_TEXT)) my_run(app, false);
    if (w.timeUp && !m.me.finished) {
        std::string msg = "Time's up: your code was submitted";
        ui_text(msg, SCREEN_W - MARGIN - 232 - 20 - ui_text_width(msg, 14), by + 12, 14, COL_CORAL);
    }

    problem_end_frame(app);
}

// ---------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------

static std::vector<std::string> split_code(const std::string &code)
{
    std::vector<std::string> lines;
    std::string line;
    for (char c : code) {
        if (c == '\n') { lines.push_back(line); line.clear(); }
        else if (c == '\t') line += "    ";
        else line += c;
    }
    if (!line.empty()) lines.push_back(line);
    // Leave out the #include lines and blank lines at the top: the interesting part is the function
    while (!lines.empty() && (lines[0].empty() || lines[0][0] == '#')) lines.erase(lines.begin());
    return lines;
}

static void draw_result(App &app)
{
    VersusState &vs = app.vs;
    VsMatch &m = vs.match;
    Workspace &w = app.work;
    if (!vs.codeSent) { send(app, "CODE " + vs_escape(eb_text(w.code))); vs.codeSent = true; }

    const Problem &p = app.practice[w.set].problems[w.problem];
    ui_header("Versus  >  " + p.title + "  >  Result");
    std::string over = "Match over at " + clock_text(vs_elapsed(m, GetTime()));
    ui_text(over, SCREEN_W - MARGIN - ui_text_width(over, 13), 26, 13, COL_MUTED);

    std::string reason;
    int outcome = vs_outcome(m, reason);
    float x = MARGIN, y = HEADER_H + 24;
    std::string eyebrow = outcome > 0 ? "WINNER" : outcome < 0 ? "WINNER" : "DRAW";
    Color ec = outcome > 0 ? COL_GREEN : outcome < 0 ? COL_BLUE : COL_AMBER;
    ui_text(eyebrow, x, y, 12, ec, FontKind::MonoBold);
    std::string title = outcome > 0 ? "You win" : outcome < 0 ? m.them.name + " wins" : "It's a draw";
    ui_text(title, x, y + 18, 38, COL_TEXT, FontKind::UiBold);
    ui_text(reason, x, y + 66, 15, COL_TEXT_SOFT);

    bool canRematch = connected(app) && !m.them.left;
    if (ui_button("Rematch, new problem", { SCREEN_W - MARGIN - 200 - 12 - 200, y + 20, 200, 46 }, canRematch ? ButtonStyle::Primary : ButtonStyle::Disabled)
        || (canRematch && IsKeyPressed(KEY_R))) {
        send(app, "REMATCH");
        m.phase = VsPhase::Lobby;
        return;
    }
    if (ui_button("Leave", { SCREEN_W - MARGIN - 200, y + 20, 200, 46 }, ButtonStyle::Outline, COL_TEXT) || IsKeyPressed(KEY_ESCAPE)) {
        leave_versus(app);
        return;
    }

    // The two players, winner first
    y += 104;
    float cardW = (SCREEN_W - 2 * MARGIN - 16) / 2.0f;
    float cardH = SCREEN_H - FOOTER_H - 16 - y;
    struct Card { const VsPlayer *p; std::string name; Color c; std::string code; bool hasCode; };
    Card mine = { &m.me, vs.myName + " (you)", COL_GREEN, eb_text(w.code), true };
    Card theirs = { &m.them, m.them.name, COL_BLUE, m.them.code, m.them.hasCode };
    Card cards[2] = { outcome < 0 ? theirs : mine, outcome < 0 ? mine : theirs };

    for (int i = 0; i < 2; i++) {
        const Card &cd = cards[i];
        const VsPlayer &pl = *cd.p;
        Rectangle r = { x + i * (cardW + 16), y, cardW, cardH };
        bool winner = (i == 0 && outcome != 0);
        ui_panel(r, COL_SURFACE, winner ? cd.c : COL_LINE, 1);
        avatar(cd.name, r.x + 38, r.y + 36, 20, cd.c);
        ui_text(cd.name, r.x + 68, r.y + 25, 17, COL_TEXT, FontKind::UiBold);
        std::string time = pl.finished ? clock_text(pl.finishSeconds) : (pl.left ? "left" : "not solved");
        ui_text(time, r.x + r.width - 20 - ui_text_width(time, 22, FontKind::MonoBold), r.y + 22, 22, pl.finished ? cd.c : COL_MUTED, FontKind::MonoBold);

        // Stats
        std::string tests = (pl.finished ? std::to_string(pl.total) : std::to_string(pl.bestPassed)) + " / " + std::to_string(pl.total);
        const char *labels[4] = { "best tests", "runs", "compile errors", "result" };
        std::string values[4] = { tests, std::to_string(pl.runs), std::to_string(pl.compileErrors),
                                  pl.finished ? "accepted" : (pl.lastResult.empty() ? "-" : pl.lastResult) };
        float sx = r.x + 20, colW = (r.width - 40) / 4;
        for (int k = 0; k < 4; k++) {
            ui_text(values[k], sx + k * colW, r.y + 72, 17, COL_TEXT, FontKind::MonoBold);
            ui_text(labels[k], sx + k * colW, r.y + 96, 12, COL_MUTED);
        }

        // Their solution
        Rectangle codeBox = { r.x + 16, r.y + 124, r.width - 32, r.height - 140 };
        if (cd.hasCode) {
            ui_code(split_code(cd.code), codeBox, 0, cd.c, false, 13);
        } else {
            ui_panel(codeBox, COL_BG, COL_LINE, 1);
            ui_text(connected(app) ? "Waiting for " + m.them.name + "'s code..." : "Their code didn't arrive (disconnected).",
                    codeBox.x + 16, codeBox.y + 16, 13, COL_MUTED);
        }
    }

    std::string score = "Head-to-head this session: " + vs.myName + " " + std::to_string(m.myWins) + " - " +
                        std::to_string(m.theirWins) + " " + m.them.name;
    ui_footer_hints({ "R rematch", "ESC leave", score });
}

// ---------------------------------------------------------------------------

void screen_versus(App &app)
{
    VersusState &vs = app.vs;
    double now = GetTime();
    update_network(app, now);
    vs_check_end(vs.match, now);

    switch (vs.match.phase) {
        case VsPhase::Lobby:     draw_lobby(app); break;
        case VsPhase::Countdown: draw_countdown(app); break;
        case VsPhase::Playing:   play_frame(app); break;
        case VsPhase::Finished:  draw_result(app); break;
    }
}
