// screen_problem.cpp - one Exam prep problem, LeetCode style:
//   left:  the description, example, rules and hints
//   right: the code editor, and under it the test results
// Run tests the code; Submit does the same and, if every test passes and no
// rule is broken, marks the problem solved with its time. Exam mode adds a
// countdown, hides the hints, and submits by itself when time runs out.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <thread>

const char *WORK_DIR = "practice_build";   // the generated test program (safe to delete)

static const Problem &current(App &app) { return app.practice[app.work.set].problems[app.work.problem]; }

static std::string two_digits(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

static std::string clock_text(double seconds)
{
    if (seconds < 0) seconds = 0;
    int s = (int)seconds;
    return two_digits(s / 60) + ":" + two_digits(s % 60);
}

static Color difficulty_color(const std::string &d)
{
    if (d == "Easy") return COL_GREEN;
    if (d == "Hard") return COL_CORAL;
    return COL_AMBER;
}

// A small rounded label, filled or outlined. Returns its width.
static float tag(const std::string &text, float x, float y, Color color, bool filled)
{
    float w = ui_text_width(text, 12, FontKind::Mono) + 14;
    if (filled) {
        DrawRectangleRounded({ x, y, w, 20 }, 0.3f, 4, color);
        ui_text(text, x + 7, y + 3, 12, COL_BG, FontKind::Mono);
    } else {
        ui_panel({ x, y, w, 20 }, COL_SURFACE, color, 1);
        ui_text(text, x + 7, y + 3, 12, color, FontKind::Mono);
    }
    return w;
}

void save_draft(App &app)
{
    Workspace &w = app.work;
    if (w.draftPath.empty() || !w.code.changed) return;
    std::error_code ec;
    std::filesystem::create_directories("saves", ec);
    std::ofstream f(w.draftPath);
    if (f) {
        f << eb_text(w.code);
        w.code.changed = false;
    }
    w.lastSaved = GetTime();
}

static void leave(App &app)
{
    save_draft(app);
    open_exam_prep(app);
}

// ---------------------------------------------------------------------------
// Running
// ---------------------------------------------------------------------------

// The last run started (it may belong to a problem that's been left): a new
// one waits until it's finished, as they share the practice_build folder.
static std::shared_ptr<RunJob> lastJob;

void problem_start_run(App &app, bool submit)
{
    Workspace &w = app.work;
    if (w.runPhase != 0) return;
    if (lastJob && !lastJob->finished) return;
    save_draft(app);
    w.runPhase = 1;
    w.submitting = submit;
    w.consoleTab = 1;   // watch it run in the terminal
    w.termScroll = 0;
    w.view.marks.clear();
    // Compile and run on another thread, so the window keeps going
    std::shared_ptr<RunJob> job = std::make_shared<RunJob>();
    w.job = lastJob = job;
    Problem p = current(app);
    std::string code = eb_text(w.code);
    std::thread([job, p, code]() {
        job->result = run_answer(p, code, WORK_DIR, &job->progress);
        job->finished = true;
    }).detach();
}

// The run finished: show its results
static void finish_run(App &app)
{
    Workspace &w = app.work;
    const Problem &p = current(app);
    w.result = w.job->result;
    w.job.reset();
    w.hasResult = true;
    // Compile errors: marked on their lines in the editor, and the Problems tab opens
    for (const CompileProblem &cp : w.result.problems) {
        if (cp.line <= 0 || cp.line > (int)w.code.lines.size()) continue;
        w.view.marks.push_back({ cp.line - 1, cp.error, cp.message, w.code.lines[cp.line - 1] });
    }
    w.consoleTab = w.result.status == RunStatus::CompileError ? 2 : w.result.status == RunStatus::NoCompiler ? 1 : 0;
    for (const CompileProblem &cp : w.result.problems)   // show the first error in the editor
        if (cp.error && cp.line > 0 && !cp.inTests) { w.view.jumpRow = cp.line - 1; w.view.jumpCol = cp.col > 0 ? cp.col - 1 : 0; break; }
    w.runCount++;
    if (w.result.status == RunStatus::Crashed) w.hadCrash = true;
    if (w.result.status == RunStatus::TimedOut) w.hadTimeout = true;
    if (!w.versus) quest_event(app, "run");
    w.resultWasSubmit = w.submitting;
    w.consoleScroll = 0;
    w.detailScroll = 0;

    // Show the first failing test's details (a crash first), or the first test
    w.selectedTest = w.result.crashedAt >= 0 ? w.result.crashedAt : 0;
    if (w.result.crashedAt < 0) {
        for (int i = 0; i < (int)w.result.tests.size(); i++)
            if (!w.result.tests[i].passed) { w.selectedTest = i; break; }
    }

    if (w.submitting && run_accepted(w.result) && !w.accepted) {
        w.accepted = true;
        int seconds = (int)(GetTime() - w.started);
        w.stoppedAt = GetTime();   // the clock stops
        auto before = app.progress.solved.find(p.id);
        int previous = before == app.progress.solved.end() ? 0 : before->second.seconds;
        bool first = progress_mark_solved(app.progress, p.id, today_string(), seconds);
        // The "Solved!" card: how long it took, and whether it's a first or a new best
        // (only on the Exam prep screen: Versus and boss fights have their own endings)
        w.finishShown = app.screen == Screen::Problem;
        w.finishAt = GetTime();
        w.firstSolve = first;
        w.solveSeconds = seconds;
        w.newBest = !first && previous > 0 && seconds < previous;
        w.bestSeconds = app.progress.solved[p.id].seconds;
        if (w.finishShown) {
            mal_react(app.mal, MalMood::Cheer, 3.0f);
            sfx_play(Sfx::LevelUp);
            fx_burst({ SCREEN_W / 2.0f, SCREEN_H / 2.0f - 40 }, 60, COL_GREEN, COL_AMBER);
        }
        learned_today(app);
        quest_event(app, "solve");
        if (w.hadCrash) catch_bug(app, "segfault", "exam prep");      // fixed a crash: caught a Segfault
        if (w.hadTimeout) catch_bug(app, "infloop", "exam prep");     // fixed an endless loop
        w.hadCrash = w.hadTimeout = false;
        if (first && !p.topic.empty()) {
            record_answer(app, p.topic, true);   // counts towards the topic's mastery, and saves
        } else {
            std::string error;
            progress_save(app.progressPath, app.progress, error);
        }
    }
    w.runPhase = 0;
}

// ---------------------------------------------------------------------------
// The "Solved!" card, over everything once a submit is accepted
// ---------------------------------------------------------------------------

// The next problem in this set after `from` (wrapping round), preferring ones
// not solved yet. -1 if this is the only one.
static int next_problem(const App &app, int set, int from)
{
    const std::vector<Problem> &ps = app.practice[set].problems;
    int n = (int)ps.size();
    for (int pass = 0; pass < 2; pass++)
        for (int k = 1; k < n; k++) {
            int i = (from + k) % n;
            if (pass == 1 || !progress_is_solved(app.progress, ps[i].id)) return i;
        }
    return -1;
}

// Returns true if it left the screen.
static bool draw_finish(App &app)
{
    Workspace &w = app.work;
    const Problem &p = current(app);
    float age = (float)(GetTime() - w.finishAt);
    float in = fminf(1.0f, age * 4);
    float pop = 1 + 0.12f * expf(-age * 6) * sinf(age * 20);   // the card bounces in

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.55f * in));
    float cw = 560 * pop, ch = 380 * pop;
    Rectangle c = { SCREEN_W / 2.0f - cw / 2, SCREEN_H / 2.0f - ch / 2 + (1 - in) * 40, cw, ch };
    ui_panel(c, COL_SURFACE, COL_GREEN, 2);

    mal_draw_with_pet(app.mal, { c.x + 110, c.y + 200 }, 100);
    float tx = c.x + 220;
    ui_text("ACCEPTED", tx, c.y + 34, 13, COL_GREEN, FontKind::MonoBold);
    ui_text("Solved!", tx, c.y + 54, 40, COL_TEXT, FontKind::UiBold);
    ui_text_wrapped(p.title, tx, c.y + 104, c.width - 240, 15, COL_TEXT_SOFT);

    // Time, and what it earned
    std::string time = clock_text(w.solveSeconds);
    ui_text(time, tx, c.y + 140, 30, COL_AMBER, FontKind::MonoBold);
    std::string sub = w.examMode ? "exam mode, " + clock_text(w.endsAt - w.started) + " allowed" : "to solve it";
    ui_text(sub, tx + ui_text_width(time, 30, FontKind::MonoBold) + 12, c.y + 152, 13, COL_MUTED);
    float ly = c.y + 192;
    if (w.firstSolve) {
        ui_text("+" + std::to_string(XP_SOLVED) + " XP", tx, ly, 18, COL_GREEN, FontKind::MonoBold);
        ui_text("first time solved", tx + 92, ly + 3, 13, COL_TEXT_SOFT);
    } else if (w.newBest) {
        ui_text("NEW BEST TIME!", tx, ly, 18, COL_AMBER, FontKind::MonoBold);
    } else {
        ui_text("Best time: " + clock_text(w.bestSeconds), tx, ly, 15, COL_TEXT_SOFT, FontKind::Mono);
    }
    std::string runs = std::to_string(w.runCount) + (w.runCount == 1 ? " run" : " runs") + ", " +
                       std::to_string(w.result.passed) + " / " + std::to_string(w.result.passed + w.result.failed) + " tests";
    ui_text(runs, tx, ly + 30, 13, COL_MUTED, FontKind::Mono);

    // Where next
    int next = next_problem(app, w.set, w.problem);
    float by = c.y + c.height - 28 - 46;
    float bw = (c.width - 56 - 24) / 3;
    bool ready = age > 0.35f;   // so a held Enter from submitting doesn't skip it
    if (next >= 0 && (ui_button("Next problem", { c.x + 28, by, bw, 46 }, ButtonStyle::Primary, COL_GREEN) || (ready && IsKeyPressed(KEY_ENTER)))) {
        save_draft(app);
        open_problem(app, w.set, next, w.examMode);
        return true;
    }
    if (ui_button("All problems", { c.x + 28 + bw + 12, by, bw, 46 }, ButtonStyle::Outline, COL_TEXT) || (ready && IsKeyPressed(KEY_ESCAPE))) {
        leave(app);
        return true;
    }
    if (ui_button("See my code", { c.x + 28 + 2 * (bw + 12), by, bw, 46 }, ButtonStyle::Outline, COL_TEXT_SOFT))
        w.finishShown = false;   // back to the editor, clock still stopped
    std::string keys = next >= 0 ? "ENTER next problem     ESC all problems" : "ESC all problems";
    ui_text(keys, c.x + c.width - 28 - ui_text_width(keys, 11, FontKind::Mono), by - 24, 11, COL_MUTED, FontKind::Mono);
    return false;
}

// ---------------------------------------------------------------------------
// Left: the description
// ---------------------------------------------------------------------------

void problem_draw_description(App &app, Rectangle r)
{
    Workspace &w = app.work;
    const Problem &p = current(app);
    const ProblemSet &set = app.practice[w.set];
    ui_panel(r, COL_SURFACE, COL_LINE, 1);

    // Scroll with the wheel when the mouse is over it
    if (CheckCollisionPointRec(GetMousePosition(), r)) w.descScroll -= GetMouseWheelMove() * 40;
    float maxScroll = w.descHeight - r.height + 24;
    if (w.descScroll > maxScroll) w.descScroll = maxScroll;
    if (w.descScroll < 0) w.descScroll = 0;

    ui_scissor((int)r.x + 1, (int)r.y + 1, (int)r.width - 2, (int)r.height - 2);
    float x = r.x + 22, w2 = r.width - 44;
    float top = r.y + 20 - w.descScroll, y = top;

    y += ui_text_wrapped(two_digits(w.problem + 1) + ". " + p.title, x, y, w2, 22, COL_TEXT, FontKind::UiBold) + 2;
    ui_text(p.function, x, y, 13, COL_MUTED, FontKind::Mono);
    y += 26;

    float tx = x;
    tx += tag(p.difficulty, tx, y, difficulty_color(p.difficulty), true) + 8;
    tx += tag(set.exam.empty() ? set.title : set.exam, tx, y, set.exam == "Exam 2" ? COL_PURPLE : COL_BLUE, false) + 8;
    if (progress_is_solved(app.progress, p.id)) tx += tag("Solved", tx, y, COL_GREEN, false) + 8;
    if (w.versus) tag("Versus", tx, y, COL_AMBER, true);
    else if (w.examMode) tag("Exam mode", tx, y, COL_AMBER, true);
    y += 36;

    for (const std::string &para : p.text) y += ui_text_wrapped(para, x, y, w2, 15, COL_TEXT_SOFT) + 8;
    y += 6;

    if (!p.example.empty()) {
        ui_text("Example", x, y, 14, COL_TEXT, FontKind::UiBold);
        y += 24;
        float h = p.example.size() * 13 * 1.55f + 20;
        ui_code(p.example, { x, y, w2, h }, 0, COL_BLUE, false, 13);
        y += h + 16;
    }

    if (!p.rules.empty()) {
        ui_text("Rules", x, y, 14, COL_TEXT, FontKind::UiBold);
        y += 24;
        for (const std::string &rule : p.rules) {
            DrawCircleV({ x + 5, y + 9 }, 3, COL_AMBER);
            y += ui_text_wrapped(rule, x + 16, y, w2 - 16, 14, COL_AMBER) + 4;
        }
        if (!p.forbids.empty()) {
            ui_text("Checked automatically when you run your code.", x + 16, y, 12, COL_MUTED);
            y += 20;
        }
        y += 10;
    }

    if (!p.hints.empty()) {
        ui_text("Hints", x, y, 14, COL_TEXT, FontKind::UiBold);
        y += 24;
        if (w.examMode) {
            ui_text(w.versus ? "Hints are off in Versus." : "Hints are off in exam mode.", x, y, 14, COL_MUTED);
            y += 24;
        } else {
            for (int i = 0; i < w.hintsShown && i < (int)p.hints.size(); i++) {
                ui_text(std::to_string(i + 1) + ".", x, y, 14, COL_BLUE, FontKind::MonoBold);
                y += ui_text_wrapped(p.hints[i], x + 22, y, w2 - 22, 14, COL_TEXT_SOFT) + 6;
            }
            if (w.hintsShown < (int)p.hints.size()) {
                std::string label = "Show hint " + std::to_string(w.hintsShown + 1) + " of " + std::to_string(p.hints.size());
                float bw = ui_text_width(label, 16, FontKind::UiBold) + 32;
                bool visible = y + 38 > r.y && y < r.y + r.height;
                if (ui_button(label, { x, y + 2, bw, 36 }, ButtonStyle::Outline, COL_BLUE) && visible) w.hintsShown++;
                y += 44;
            }
        }
    }
    EndScissorMode();
    w.descHeight = y - top;
}

// ---------------------------------------------------------------------------
// Right, bottom: the test results
// ---------------------------------------------------------------------------

// A block of monospaced lines (expected / got values can span several lines).
static float draw_value(const std::string &label, const std::string &value, float x, float y, float w, Color color)
{
    ui_text(label, x, y, 12, COL_MUTED, FontKind::UiBold);
    y += 18;
    std::vector<std::string> lines;
    size_t start = 0;
    while (true) {
        size_t nl = value.find('\n', start);
        lines.push_back(value.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    float h = lines.size() * 19 + 12;
    ui_panel({ x, y, w, h }, COL_BG, COL_LINE_SOFT, 1);
    for (size_t i = 0; i < lines.size(); i++) {
        std::string shown = lines[i].empty() && lines.size() > 1 ? "(empty line)" : lines[i];
        ui_text(shown, x + 10, y + 6 + i * 19, 14, lines[i].empty() && lines.size() > 1 ? COL_MUTED : color, FontKind::Mono);
    }
    return 18 + h + 10;
}

// A round icon for a test: a tick, a cross, ! for a crash, a clock for a hang, - for not run
static void draw_outcome(float x, float y, TestOutcome o)
{
    Color c = o == TestOutcome::Passed ? COL_GREEN : o == TestOutcome::TimedOut ? COL_AMBER : o == TestOutcome::NotRun ? COL_MUTED : COL_CORAL;
    DrawCircleV({ x, y }, 7, c);
    switch (o) {
        case TestOutcome::Passed:
            DrawLineEx({ x - 3.5f, y }, { x - 1, y + 2.5f }, 2, COL_BG);
            DrawLineEx({ x - 1, y + 2.5f }, { x + 3.5f, y - 2.5f }, 2, COL_BG);
            break;
        case TestOutcome::Failed:
            DrawLineEx({ x - 2.5f, y - 2.5f }, { x + 2.5f, y + 2.5f }, 2, COL_BG);
            DrawLineEx({ x + 2.5f, y - 2.5f }, { x - 2.5f, y + 2.5f }, 2, COL_BG);
            break;
        case TestOutcome::Crashed:
            DrawLineEx({ x, y - 4 }, { x, y + 1 }, 2, COL_BG);
            DrawCircleV({ x, y + 3.5f }, 1.2f, COL_BG);
            break;
        case TestOutcome::TimedOut:
            DrawCircleLines((int)x, (int)y, 4.5f, COL_BG);
            DrawLineEx({ x, y }, { x, y - 3 }, 1.5f, COL_BG);
            DrawLineEx({ x, y }, { x + 2.5f, y }, 1.5f, COL_BG);
            break;
        case TestOutcome::NotRun:
            DrawLineEx({ x - 3, y }, { x + 3, y }, 2, COL_BG);
            break;
    }
}


static TestOutcome outcome_of_state(int s)
{
    switch (s) {
        case 1: return TestOutcome::Passed;
        case 2: return TestOutcome::Failed;
        case 3: return TestOutcome::Crashed;
        case 4: return TestOutcome::TimedOut;
        default: return TestOutcome::NotRun;
    }
}

// The colour of a terminal line, from how it starts
static Color term_colour(const std::string &line)
{
    if (line.compare(0, 1, "$") == 0) return COL_BLUE;
    if (line.compare(0, 6, "  PASS") == 0) return COL_GREEN;
    if (line.compare(0, 6, "  FAIL") == 0 || line.compare(0, 7, "  CRASH") == 0 || line.compare(0, 7, "  error") == 0) return COL_CORAL;
    if (line.compare(0, 6, "  TIME") == 0) return COL_AMBER;
    if (line.compare(0, 3, "  |") == 0) return COL_TEXT;
    if (line.compare(0, 5, "Done:") == 0 || line.compare(0, 5, "Didn'") == 0) return COL_TEXT;
    return COL_MUTED;
}

// The terminal: every line of the run, newest at the bottom (it follows the
// end unless scrolled up). Long lines wrap.
static void draw_terminal(Workspace &w, const std::vector<std::string> &log, Rectangle body, bool running)
{
    const float lineH = 16, size = 13;
    float charW = ui_text_width("M", size, FontKind::Mono);
    int perRow = charW > 0 ? (int)((body.width - 30) / charW) : 100;
    if (perRow < 20) perRow = 20;
    // Wrapped rows
    std::vector<std::pair<std::string, Color>> rows;
    for (const std::string &line : log) {
        Color c = term_colour(line);
        size_t at = 0;
        do {
            rows.push_back({ (at > 0 ? "    " : "") + line.substr(at, at == 0 ? perRow : perRow - 4), c });
            at += at == 0 ? perRow : perRow - 4;
        } while (at < line.size());
    }
    if (running) {
        static const char *spin[] = { "|", "/", "-", "\\" };
        rows.push_back({ std::string(spin[(int)(GetTime() * 10) % 4]) + " ", COL_AMBER });
    }
    int shown = (int)((body.height - 12) / lineH);
    int maxScroll = (int)rows.size() - shown;
    if (maxScroll < 0) maxScroll = 0;
    if (CheckCollisionPointRec(GetMousePosition(), body)) w.termScroll += GetMouseWheelMove() * 3;
    if (w.termScroll > maxScroll) w.termScroll = (float)maxScroll;
    if (w.termScroll < 0) w.termScroll = 0;
    int first = maxScroll - (int)w.termScroll;
    DrawRectangleRec(body, COL_BG);
    ui_scissor((int)body.x, (int)body.y, (int)body.width, (int)body.height);
    for (int i = 0; i < shown + 1 && first + i < (int)rows.size(); i++)
        ui_text(rows[first + i].first, body.x + 16, body.y + 8 + i * lineH, size, rows[first + i].second, FontKind::Mono);
    EndScissorMode();
}

// The Problems tab: each compiler error, click to jump to its line
static void draw_problems(App &app, const RunResult &res, Rectangle body)
{
    Workspace &w = app.work;
    float x = body.x + 16, y = body.y + 8;
    ui_scissor((int)body.x, (int)body.y, (int)body.width, (int)body.height);
    bool tests = false;
    for (const CompileProblem &cp : res.problems) if (cp.inTests) tests = true;
    if (tests) {
        y += ui_text_wrapped("The tests couldn't call your code: check the function's name and parameters match the prototype: " +
                             current(app).prototype, x, y, body.width - 32, 13, COL_AMBER) + 6;
    }
    if (res.problems.empty()) {
        ui_text_wrapped("The compiler didn't say where. See the Terminal tab for what it printed.", x, y, body.width - 32, 13, COL_MUTED);
    }
    for (const CompileProblem &cp : res.problems) {
        if (y > body.y + body.height) break;
        Rectangle row = { body.x + 6, y - 2, body.width - 12, 22 };
        bool canJump = cp.line > 0 && !cp.inTests;
        bool hover = canJump && CheckCollisionPointRec(GetMousePosition(), row);
        if (hover) {
            DrawRectangleRounded(row, 0.3f, 4, COL_SURFACE_2);
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { w.view.jumpRow = cp.line - 1; w.view.jumpCol = cp.col > 0 ? cp.col - 1 : 0; }
        }
        Color c = cp.error ? COL_CORAL : COL_AMBER;
        DrawCircleV({ x + 6, y + 9 }, 6, c);
        ui_text(cp.error ? "x" : "!", x + 3, y + 1, 11, COL_BG, FontKind::MonoBold);
        std::string where = cp.inTests ? "tests" : cp.line > 0 ? "line " + std::to_string(cp.line) + (cp.col > 0 ? ":" + std::to_string(cp.col) : "") : "";
        ui_text(where, x + 20, y + 2, 13, COL_MUTED, FontKind::Mono);
        std::string msg = cp.message;
        float mx = x + 20 + 90, room = body.x + body.width - 16 - mx;
        while (msg.size() > 4 && ui_text_width(msg, 13, FontKind::Mono) > room) msg = msg.substr(0, msg.size() - 4) + "...";
        ui_text(msg, mx, y + 2, 13, COL_TEXT, FontKind::Mono);
        y += 24;
    }
    EndScissorMode();
}

void problem_draw_console(App &app, Rectangle r)
{
    Workspace &w = app.work;
    const RunResult &res = w.result;
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    float x = r.x + 18, y = r.y + 12;
    bool running = w.runPhase > 0 && w.job;
    Rectangle body = { r.x + 1, r.y + 41, r.width - 2, r.height - 42 };

    // Tabs, top left: Tests, Terminal, and Problems when it didn't compile
    bool hasProblems = w.hasResult && !running && res.status == RunStatus::CompileError;
    int errors = 0;
    for (const CompileProblem &cp : res.problems) if (cp.error) errors++;
    std::vector<std::string> tabs = { "Tests", "Terminal" };
    if (hasProblems) tabs.push_back("Problems " + std::to_string(errors > 0 ? errors : (int)res.problems.size()));
    if (!hasProblems && w.consoleTab == 2) w.consoleTab = 0;
    float tx = x - 6;
    for (int i = 0; i < (int)tabs.size(); i++) {
        float tw = ui_text_width(tabs[i], 14, FontKind::UiBold) + 20;
        Rectangle tr = { tx, r.y + 6, tw, 30 };
        bool on = w.consoleTab == i, hov = CheckCollisionPointRec(GetMousePosition(), tr);
        Color tc = i == 2 ? COL_CORAL : on ? COL_TEXT : (hov ? COL_TEXT_SOFT : COL_MUTED);
        ui_text(tabs[i], tx + 10, y, 14, tc, FontKind::UiBold);
        if (on) DrawRectangle((int)tx + 6, (int)(r.y + 38), (int)tw - 12, 2, i == 2 ? COL_CORAL : COL_AMBER);
        if (hov) { ui_want_hand(); if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { w.consoleTab = i; w.termScroll = 0; } }
        tx += tw + 4;
    }
    DrawLine((int)r.x + 1, (int)(r.y + 40), (int)(r.x + r.width - 1), (int)(r.y + 40), COL_LINE_SOFT);

    // ----- Running: what it's doing, a box per test filling in, and the terminal -----
    if (running) {
        std::vector<std::string> log;
        std::vector<int> state;
        std::string phase;
        {
            std::lock_guard<std::mutex> g(w.job->progress.lock);
            log = w.job->progress.log;
            state = w.job->progress.state;
            phase = w.job->progress.phase;
        }
        if (phase.empty()) phase = "Starting";
        float bx = r.x + r.width - 18;
        for (int i = (int)state.size() - 1; i >= 0; i--) {   // the boxes, right to left
            bx -= 14;
            Color c = state[i] == 1 ? COL_GREEN : state[i] == 2 || state[i] == 3 ? COL_CORAL : state[i] == 4 ? COL_AMBER : state[i] == 5 ? COL_MUTED : COL_LINE;
            DrawRectangleRounded({ bx, y + 2, 11, 14 }, 0.3f, 4, c);
        }
        std::string ph = (w.submitting ? "Submitting: " : "") + phase + "...";
        ui_text(ph, bx - 14 - ui_text_width(ph, 13, FontKind::Mono), y + 2, 13, COL_AMBER, FontKind::Mono);
        if (w.consoleTab == 0) {
            // the tests so far, live
            ui_scissor((int)body.x, (int)body.y, (int)body.width, (int)body.height);
            float ry = body.y + 8;
            for (int i = 0; i < (int)state.size() && ry < body.y + body.height; i++, ry += 22) {
                if (state[i] == 0) {
                    DrawCircleLines((int)(body.x + 20), (int)(ry + 9), 7, COL_LINE);
                    ui_text("Test " + std::to_string(i + 1), body.x + 34, ry + 1, 13, COL_MUTED);
                } else {
                    draw_outcome(body.x + 20, ry + 9, outcome_of_state(state[i]));
                    ui_text("Test " + std::to_string(i + 1), body.x + 34, ry + 1, 13, COL_TEXT_SOFT);
                }
            }
            EndScissorMode();
        } else {
            draw_terminal(w, log, body, true);
        }
        return;
    }
    if (!w.hasResult) {
        ui_text_wrapped("Run your code to see the results here (Ctrl+Enter or F5). Run checks your code against the tests; "
                        "Submit does the same and marks the problem solved when every test passes. Each test is run on its own, "
                        "so a crash or an endless loop only stops the test it's in.",
                        x, r.y + 56, r.width - 36, 14, COL_MUTED);
        return;
    }

    // The verdict, top right of the console
    std::string verdict, detail;
    Color vc = COL_CORAL;
    int total = (int)res.tests.size();
    std::string count = std::to_string(res.passed) + " / " + std::to_string(total) + " tests passed";
    std::string where = res.crashedAt >= 0 ? "test " + std::to_string(res.crashedAt + 1) : "a test";
    switch (res.status) {
        case RunStatus::Passed:
            if (!res.ruleProblems.empty()) { verdict = "Rule broken"; vc = COL_AMBER; detail = count + ", but the code breaks a rule"; }
            else if (w.resultWasSubmit) { verdict = "Accepted"; vc = COL_GREEN; detail = count; }
            else { verdict = "All tests passed"; vc = COL_GREEN; detail = "Submit to mark it solved"; }
            break;
        case RunStatus::Failed:       verdict = "Wrong answer"; detail = count; break;
        case RunStatus::CompileError: verdict = "Compile error"; detail = std::to_string(errors) + (errors == 1 ? " error" : " errors") + ": click one to go to its line"; break;
        case RunStatus::Crashed:      verdict = "Runtime error"; detail = "crashed in " + where + ", " + count; break;
        case RunStatus::TimedOut:     verdict = "Time limit exceeded"; vc = COL_AMBER; detail = where + " never finished, " + count; break;
        case RunStatus::NoCompiler:   verdict = "No compiler"; detail = ""; break;
    }
    float vw = ui_text_width(verdict, 16, FontKind::UiBold);
    float dw = ui_text_width(detail, 13);
    ui_text(detail, r.x + r.width - 18 - dw, y + 2, 13, COL_TEXT_SOFT);
    ui_text(verdict, r.x + r.width - 18 - dw - 14 - vw, y, 16, vc, FontKind::UiBold);

    if (w.consoleTab == 1) { draw_terminal(w, res.log, body, false); return; }
    if (w.consoleTab == 2) { draw_problems(app, res, body); return; }
    if (res.status == RunStatus::CompileError || res.status == RunStatus::NoCompiler) {
        ui_text_wrapped(res.status == RunStatus::NoCompiler ? res.compilerOutput
                        : "It didn't compile, so no tests ran. The Problems tab lists the errors, and they're marked on their lines in the editor.",
                        x, body.y + 12, body.width - 36, 14, COL_TEXT_SOFT);
        return;
    }

    // The list of tests on the left
    float listW = 300;
    Rectangle list = { body.x, body.y, listW, body.height };
    int rowsShown = (int)((list.height - 8) / 24);
    if (CheckCollisionPointRec(GetMousePosition(), list)) w.consoleScroll -= GetMouseWheelMove() * 2;
    float maxScroll = (float)res.tests.size() - rowsShown;
    if (w.consoleScroll > maxScroll) w.consoleScroll = maxScroll;
    if (w.consoleScroll < 0) w.consoleScroll = 0;

    ui_scissor((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    for (int i = (int)w.consoleScroll; i < (int)res.tests.size(); i++) {
        float ry = list.y + 6 + (i - (int)w.consoleScroll) * 24;
        if (ry > list.y + list.height) break;
        Rectangle row = { list.x + 6, ry, listW - 12, 22 };
        bool hover = CheckCollisionPointRec(GetMousePosition(), row);
        if (hover) { ui_want_hand(); if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { w.selectedTest = i; w.detailScroll = 0; } }
        if (i == w.selectedTest) DrawRectangleRounded(row, 0.3f, 4, COL_SURFACE_2);
        const TestResult &t = res.tests[i];
        draw_outcome(row.x + 14, row.y + 11, t.outcome);
        std::string label = "Test " + std::to_string(i + 1) + "  " + t.description;
        while (label.size() > 4 && ui_text_width(label, 13) > listW - 50) label = label.substr(0, label.size() - 4) + "...";
        Color lc = i == w.selectedTest ? COL_TEXT : t.outcome == TestOutcome::NotRun ? COL_MUTED : COL_TEXT_SOFT;
        ui_text(label, row.x + 28, row.y + 3, 13, lc);
    }
    EndScissorMode();
    DrawLine((int)(list.x + listW), (int)list.y, (int)(list.x + listW), (int)(list.y + list.height), COL_LINE_SOFT);

    // The selected test's details on the right (scrolls when they don't fit)
    Rectangle details = { list.x + listW + 1, body.y, body.width - listW - 2, body.height };
    if (CheckCollisionPointRec(GetMousePosition(), details)) w.detailScroll -= GetMouseWheelMove() * 30;
    float maxDetail = w.detailHeight - details.height + 16;
    if (w.detailScroll > maxDetail) w.detailScroll = maxDetail;
    if (w.detailScroll < 0) w.detailScroll = 0;

    float dx = details.x + 17, dw2 = details.x + details.width - dx - 18;
    float dtop = body.y + 10 - w.detailScroll, dy = dtop;
    ui_scissor((int)details.x, (int)details.y, (int)details.width, (int)details.height);
    for (const std::string &rule : res.ruleProblems)
        dy += ui_text_wrapped(rule, dx, dy, dw2, 13, COL_AMBER) + 6;
    if (w.selectedTest < (int)res.tests.size()) {
        const TestResult &t = res.tests[w.selectedTest];
        dy += ui_text_wrapped(t.description, dx, dy, dw2, 14, COL_TEXT, FontKind::UiBold) + 8;
        if (t.outcome == TestOutcome::Passed) {
            ui_text("Passed", dx, dy, 14, COL_GREEN);
            dy += 26;
        } else if (t.outcome == TestOutcome::Failed) {
            // Expected and got side by side, so patterns and grids line up
            float colW = (dw2 - 12) / 2;
            float h1 = draw_value("Expected", t.expected, dx, dy, colW, COL_GREEN);
            float h2 = draw_value("Your code gave", t.got, dx + colW + 12, dy, colW, COL_CORAL);
            dy += (h1 > h2 ? h1 : h2);
        } else {
            // Crashed, hung or not run: what happened, why it usually happens, and what the test was doing
            bool hung = t.outcome == TestOutcome::TimedOut, notRun = t.outcome == TestOutcome::NotRun;
            Color c = hung ? COL_AMBER : notRun ? COL_MUTED : COL_CORAL;
            std::string head = hung ? "Never finished (stopped after " + std::to_string(TEST_SECONDS) + " seconds)"
                             : notRun ? "Not run: the code before it crashed or hung every time"
                             : "Crashed: " + t.crash;
            ui_text(head, dx, dy, 14, c, FontKind::UiBold);
            dy += 22;
            if (!notRun) dy += ui_text_wrapped(crash_explanation(t.crash), dx, dy, dw2, 13, COL_TEXT_SOFT) + 10;
            if (!t.code.empty()) {
                ui_text("What the test was doing (the last line is the check):", dx, dy, 12, COL_MUTED, FontKind::UiBold);
                dy += 18;
                std::vector<std::string> lines;
                size_t at = 0;
                while (at <= t.code.size()) {
                    size_t nl = t.code.find('\n', at);
                    lines.push_back(t.code.substr(at, nl == std::string::npos ? std::string::npos : nl - at));
                    if (nl == std::string::npos) break;
                    at = nl + 1;
                }
                float h = lines.size() * 19 + 12;
                ui_panel({ dx, dy, dw2, h }, COL_BG, Fade(c, 0.6f), 1);
                int cols = (int)((dw2 - 20) / ui_text_width("M", 13, FontKind::Mono));
                for (size_t i = 0; i < lines.size(); i++)
                    editor_draw_code_line(lines[i], dx + 10, dy + 6 + i * 19, 13, 0, cols);
                dy += h + 10;
            }
        }
    }
    if (!res.programOutput.empty()) {
        std::string out = res.programOutput;
        if (out.size() > 600) out = out.substr(0, 600) + "...";
        while (!out.empty() && out.back() == '\n') out.pop_back();
        dy += draw_value("What your code printed", out, dx, dy, dw2, COL_TEXT_SOFT);
    }
    w.detailHeight = dy - dtop;
    EndScissorMode();
}

// The editor, with its bar (file name, Reset code) above it
void problem_draw_editor(App &app, Rectangle editorBox, bool locked)
{
    Workspace &w = app.work;
    const Problem &p = current(app);
    ui_panel(editorBox, COL_SURFACE, COL_LINE, 1);
    ui_text("solution.cpp", editorBox.x + 18, editorBox.y + 11, 13, COL_TEXT, FontKind::Mono);
    std::string lang = w.versus ? "only you can see this" : "C / C++";
    ui_text(lang, editorBox.x + 18 + ui_text_width("solution.cpp", 13, FontKind::Mono) + 16, editorBox.y + 11, 13, COL_MUTED, FontKind::Mono);
    if (ui_button("Reset code", { editorBox.x + editorBox.width - 12 - 110, editorBox.y + 5, 110, 28 }, locked ? ButtonStyle::Disabled : ButtonStyle::Outline, COL_TEXT_SOFT)) {
        std::string starter;
        for (const std::string &line : p.starter) starter += line + "\n";
        eb_replace_all(w.code, starter);   // Ctrl+Z brings your code back
    }
    Rectangle editArea = { editorBox.x + 1, editorBox.y + 38, editorBox.width - 2, editorBox.height - 39 };
    editor_update_draw(w.code, w.view, editArea, w.runPhase == 0, locked);
}

// Call every frame: picks up the run's results once it's finished.
void problem_run_pending(App &app)
{
    Workspace &w = app.work;
    if (w.runPhase == 1 && w.job && w.job->finished) finish_run(app);
    else if (w.runPhase == 1 && !w.job) w.runPhase = 0;
}

// (Runs used to block the window for a frame; nothing to do at the end now.)
void problem_end_frame(App &app)
{
    (void)app;
}

// ---------------------------------------------------------------------------

void screen_problem(App &app)
{
    Workspace &w = app.work;
    if (w.set >= (int)app.practice.size() || w.problem >= (int)app.practice[w.set].problems.size()) { open_exam_prep(app); return; }
    const Problem &p = current(app);
    const ProblemSet &set = app.practice[w.set];

    problem_run_pending(app);

    // Exam mode: time's up submits whatever is there
    if (w.examMode && !w.timeUp && !w.accepted && GetTime() >= w.endsAt) {
        w.timeUp = true;
        problem_start_run(app, true);
    }

    bool cmd = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool locked = w.timeUp || w.finishShown;   // the editor doesn't type behind the "Solved!" card
    if (IsKeyPressed(KEY_ESCAPE) && !w.finishShown) { leave(app); return; }
    if (!locked && ((cmd && IsKeyPressed(KEY_ENTER)) || IsKeyPressed(KEY_F5) || IsKeyPressed(KEY_F6)))
        problem_start_run(app, shift || IsKeyPressed(KEY_F6));

    if (GetTime() - w.lastSaved > 2.0) save_draft(app);

    // ----- Header -----
    ui_header("Exam prep  >  " + set.title + "  >  " + p.title);
    if (ui_button("Problems", { SCREEN_W - MARGIN - 120, 12, 120, 40 }, ButtonStyle::Outline, COL_TEXT)) { leave(app); return; }
    float clockX = SCREEN_W - MARGIN - 120 - 16;
    if (w.examMode) {
        double left = (w.stoppedAt > 0 ? w.endsAt - w.stoppedAt : w.endsAt - GetTime());
        std::string t = w.timeUp ? "Time's up" : clock_text(left);
        Color c = (w.timeUp || left < 60) ? COL_CORAL : COL_AMBER;
        float tw = ui_text_width(t, 18, FontKind::MonoBold) + 28;
        ui_panel({ clockX - tw, 14, tw, 36 }, COL_BG, c, 1);
        ui_text(t, clockX - tw + 14, 22, 18, c, FontKind::MonoBold);
    } else {
        std::string t = clock_text((w.stoppedAt > 0 ? w.stoppedAt : GetTime()) - w.started);   // stops when it's solved
        float tw = ui_text_width(t, 15, FontKind::Mono);
        ui_text(t, clockX - tw, 24, 15, w.stoppedAt > 0 ? COL_GREEN : COL_MUTED, FontKind::Mono);
    }

    // ----- Layout -----
    float top = HEADER_H + 16, bottom = SCREEN_H - FOOTER_H - 12;
    Rectangle desc = { MARGIN, top, 430, bottom - top };
    float rx = desc.x + desc.width + 16, rw = SCREEN_W - MARGIN - rx;
    Rectangle editorBox = { rx, top, rw, 342 };
    Rectangle console = { rx, editorBox.y + editorBox.height + 12, rw, bottom - (editorBox.y + editorBox.height + 12) };

    problem_draw_description(app, desc);
    problem_draw_editor(app, editorBox, locked);

    problem_draw_console(app, console);

    // ----- Footer -----
    if (!w.finishShown) ui_footer_hints({ "Ctrl/Cmd+Enter run", "Ctrl/Cmd+Shift+Enter submit", "ESC problems" });
    float by = SCREEN_H - FOOTER_H + 8;
    ButtonStyle runStyle = (locked || w.runPhase != 0) ? ButtonStyle::Disabled : ButtonStyle::Outline;
    ButtonStyle submitStyle = (locked || w.runPhase != 0) ? ButtonStyle::Disabled : ButtonStyle::Primary;
    if (ui_button("Submit", { SCREEN_W - MARGIN - 120, by, 120, 40 }, submitStyle)) problem_start_run(app, true);
    if (ui_button("Run", { SCREEN_W - MARGIN - 120 - 12 - 100, by, 100, 40 }, runStyle, COL_TEXT)) problem_start_run(app, false);
    if (w.accepted) {
        std::string msg = w.examMode ? "Solved in " + clock_text(w.stoppedAt - w.started) : "Solved";
        float mw = ui_text_width(msg, 15, FontKind::UiBold);
        ui_text(msg, SCREEN_W - MARGIN - 120 - 12 - 100 - 20 - mw, by + 11, 15, COL_GREEN, FontKind::UiBold);
    } else if (w.timeUp) {
        std::string msg = "Time's up: your code was submitted";
        float mw = ui_text_width(msg, 14);
        ui_text(msg, SCREEN_W - MARGIN - 120 - 12 - 100 - 20 - mw, by + 12, 14, COL_CORAL);
    }

    if (w.finishShown && draw_finish(app)) return;

    // "Running" shows for one frame before the (blocking) run starts
    problem_end_frame(app);
}
