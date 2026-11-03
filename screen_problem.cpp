// screen_problem.cpp - one Exam prep problem, LeetCode style:
//   left:  the description, example, rules and hints
//   right: the code editor, and under it the test results
// Run tests the code; Submit does the same and, if every test passes and no
// rule is broken, marks the problem solved with its time. Exam mode adds a
// countdown, hides the hints, and submits by itself when time runs out.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include <cmath>
#include <fstream>
#include <filesystem>

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

static void start_run(App &app, bool submit)
{
    Workspace &w = app.work;
    if (w.runPhase != 0) return;
    w.runPhase = 1;          // this frame shows "Running"; the next frame runs it
    w.submitting = submit;
}

static void do_run(App &app)
{
    Workspace &w = app.work;
    const Problem &p = current(app);
    save_draft(app);
    w.result = run_answer(p, eb_text(w.code), WORK_DIR);
    w.hasResult = true;
    w.resultWasSubmit = w.submitting;
    w.consoleScroll = 0;
    w.detailScroll = 0;

    // Show the first failing test's details, or the first test
    w.selectedTest = 0;
    for (int i = 0; i < (int)w.result.tests.size(); i++)
        if (!w.result.tests[i].passed) { w.selectedTest = i; break; }

    if (w.submitting && run_accepted(w.result)) {
        w.accepted = true;
        int seconds = (int)(GetTime() - w.started);
        if (w.examMode) w.stoppedAt = GetTime();
        bool first = progress_mark_solved(app.progress, p.id, today_string(), seconds);
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
// Left: the description
// ---------------------------------------------------------------------------

static void draw_description(App &app, Rectangle r)
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

    BeginScissorMode((int)r.x + 1, (int)r.y + 1, (int)r.width - 2, (int)r.height - 2);
    float x = r.x + 22, w2 = r.width - 44;
    float top = r.y + 20 - w.descScroll, y = top;

    y += ui_text_wrapped(two_digits(w.problem + 1) + ". " + p.title, x, y, w2, 22, COL_TEXT, FontKind::UiBold) + 2;
    ui_text(p.function, x, y, 13, COL_MUTED, FontKind::Mono);
    y += 26;

    float tx = x;
    tx += tag(p.difficulty, tx, y, difficulty_color(p.difficulty), true) + 8;
    tx += tag(set.exam.empty() ? set.title : set.exam, tx, y, set.exam == "Exam 2" ? COL_PURPLE : COL_BLUE, false) + 8;
    if (progress_is_solved(app.progress, p.id)) tx += tag("Solved", tx, y, COL_GREEN, false) + 8;
    if (w.examMode) tag("Exam mode", tx, y, COL_AMBER, true);
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
            ui_text("Hints are off in exam mode.", x, y, 14, COL_MUTED);
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

static void draw_tick(float x, float y, bool ok)
{
    if (ok) {
        DrawCircleV({ x, y }, 7, COL_GREEN);
        DrawLineEx({ x - 3.5f, y }, { x - 1, y + 2.5f }, 2, COL_BG);
        DrawLineEx({ x - 1, y + 2.5f }, { x + 3.5f, y - 2.5f }, 2, COL_BG);
    } else {
        DrawCircleV({ x, y }, 7, COL_CORAL);
        DrawLineEx({ x - 2.5f, y - 2.5f }, { x + 2.5f, y + 2.5f }, 2, COL_BG);
        DrawLineEx({ x + 2.5f, y - 2.5f }, { x - 2.5f, y + 2.5f }, 2, COL_BG);
    }
}

static void draw_console(App &app, Rectangle r)
{
    Workspace &w = app.work;
    const RunResult &res = w.result;
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    float x = r.x + 18, y = r.y + 12;

    ui_text("Test results", x, y, 14, COL_TEXT, FontKind::UiBold);
    DrawLine((int)r.x + 1, (int)(r.y + 40), (int)(r.x + r.width - 1), (int)(r.y + 40), COL_LINE_SOFT);

    if (w.runPhase > 0) {
        ui_text(w.submitting ? "Submitting: compiling and running every test..." : "Compiling and running the tests...",
                x, r.y + 58, 15, COL_AMBER, FontKind::Mono);
        return;
    }
    if (!w.hasResult) {
        ui_text_wrapped("Run your code to see the results here. Run checks your code against the tests; "
                        "Submit does the same and marks the problem solved when every test passes.",
                        x, r.y + 56, r.width - 36, 14, COL_MUTED);
        return;
    }

    // The verdict, top right of the console
    std::string verdict, detail;
    Color vc = COL_CORAL;
    int total = res.passed + res.failed;
    std::string count = std::to_string(res.passed) + " / " + std::to_string(total) + " tests passed";
    switch (res.status) {
        case RunStatus::Passed:
            if (!res.ruleProblems.empty()) { verdict = "Rule broken"; vc = COL_AMBER; detail = count + ", but the code breaks a rule"; }
            else if (w.resultWasSubmit) { verdict = "Accepted"; vc = COL_GREEN; detail = count; }
            else { verdict = "All tests passed"; vc = COL_GREEN; detail = "Submit to mark it solved"; }
            break;
        case RunStatus::Failed:       verdict = "Wrong answer"; detail = count; break;
        case RunStatus::CompileError: verdict = "Compile error"; detail = "fix the first error, then run again"; break;
        case RunStatus::Crashed:
            verdict = "Runtime error";
            detail = total == 0 ? "it crashed before any test finished" : "it crashed after " + std::to_string(total) + (total == 1 ? " test" : " tests");
            break;
        case RunStatus::TimedOut:     verdict = "Time limit exceeded"; detail = "stopped after 5 seconds: a loop that never ends?"; break;
        case RunStatus::NoCompiler:   verdict = "No compiler"; detail = ""; break;
    }
    float vw = ui_text_width(verdict, 16, FontKind::UiBold);
    float dw = ui_text_width(detail, 13);
    ui_text(detail, r.x + r.width - 18 - dw, y + 2, 13, COL_TEXT_SOFT);
    ui_text(verdict, r.x + r.width - 18 - dw - 14 - vw, y, 16, vc, FontKind::UiBold);

    Rectangle body = { r.x + 1, r.y + 41, r.width - 2, r.height - 42 };

    // Compiler errors (or no compiler): show what the compiler said
    if (res.status == RunStatus::CompileError || res.status == RunStatus::NoCompiler) {
        if (CheckCollisionPointRec(GetMousePosition(), body)) w.consoleScroll -= GetMouseWheelMove() * 3;
        if (w.consoleScroll < 0) w.consoleScroll = 0;
        BeginScissorMode((int)body.x, (int)body.y, (int)body.width, (int)body.height);
        float ly = body.y + 10;
        if (res.compilerOutput.find("tests:") != std::string::npos) {
            ly += ui_text_wrapped("The tests couldn't call your code. Check the function's name and parameters match the "
                                  "prototype: " + current(app).prototype, x, ly, body.width - 36, 13, COL_AMBER) + 6;
        }
        std::vector<std::string> lines;
        size_t start = 0;
        while (start < res.compilerOutput.size()) {
            size_t nl = res.compilerOutput.find('\n', start);
            if (nl == std::string::npos) nl = res.compilerOutput.size();
            lines.push_back(res.compilerOutput.substr(start, nl - start));
            start = nl + 1;
        }
        for (size_t i = (size_t)w.consoleScroll; i < lines.size(); i++) {
            if (ly > body.y + body.height) break;
            Color c = lines[i].find("error") != std::string::npos ? COL_CORAL : COL_TEXT_SOFT;
            if (res.status == RunStatus::NoCompiler) ly += ui_text_wrapped(lines[i], x, ly, body.width - 36, 13, c, FontKind::Mono);
            else { ui_text(lines[i], x, ly, 13, c, FontKind::Mono); ly += 18; }
        }
        EndScissorMode();
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

    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    for (int i = (int)w.consoleScroll; i < (int)res.tests.size(); i++) {
        float ry = list.y + 6 + (i - (int)w.consoleScroll) * 24;
        if (ry > list.y + list.height) break;
        Rectangle row = { list.x + 6, ry, listW - 12, 22 };
        bool hover = CheckCollisionPointRec(GetMousePosition(), row);
        if (hover) { ui_want_hand(); if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { w.selectedTest = i; w.detailScroll = 0; } }
        if (i == w.selectedTest) DrawRectangleRounded(row, 0.3f, 4, COL_SURFACE_2);
        draw_tick(row.x + 14, row.y + 11, res.tests[i].passed);
        std::string label = "Test " + std::to_string(i + 1) + "  " + res.tests[i].description;
        while (label.size() > 4 && ui_text_width(label, 13) > listW - 50) label = label.substr(0, label.size() - 4) + "...";
        ui_text(label, row.x + 28, row.y + 3, 13, i == w.selectedTest ? COL_TEXT : COL_TEXT_SOFT);
    }
    if (res.status == RunStatus::Crashed || res.status == RunStatus::TimedOut) {
        float ry = list.y + 6 + ((int)res.tests.size() - (int)w.consoleScroll) * 24;
        draw_tick(list.x + 20, ry + 11, false);
        ui_text(res.status == RunStatus::Crashed ? "the next test crashed" : "the next test never finished",
                list.x + 34, ry + 3, 13, COL_CORAL);
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
    BeginScissorMode((int)details.x, (int)details.y, (int)details.width, (int)details.height);
    for (const std::string &rule : res.ruleProblems)
        dy += ui_text_wrapped(rule, dx, dy, dw2, 13, COL_AMBER) + 6;
    if (w.selectedTest < (int)res.tests.size()) {
        const TestResult &t = res.tests[w.selectedTest];
        dy += ui_text_wrapped(t.description, dx, dy, dw2, 14, COL_TEXT, FontKind::UiBold) + 8;
        if (t.passed) {
            ui_text("Passed", dx, dy, 14, COL_GREEN);
            dy += 26;
        } else {
            // Expected and got side by side, so patterns and grids line up
            float colW = (dw2 - 12) / 2;
            float h1 = draw_value("Expected", t.expected, dx, dy, colW, COL_GREEN);
            float h2 = draw_value("Your code gave", t.got, dx + colW + 12, dy, colW, COL_CORAL);
            dy += (h1 > h2 ? h1 : h2);
        }
    } else if (res.tests.empty() && res.status == RunStatus::Crashed) {
        dy += ui_text_wrapped("Your code crashed before the first test finished. A pointer to nothing (nullptr), "
                              "or reading past the end of an array, are the usual causes.", dx, dy, dw2, 14, COL_TEXT_SOFT) + 8;
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

// ---------------------------------------------------------------------------

void screen_problem(App &app)
{
    Workspace &w = app.work;
    if (w.set >= (int)app.practice.size() || w.problem >= (int)app.practice[w.set].problems.size()) { open_exam_prep(app); return; }
    const Problem &p = current(app);
    const ProblemSet &set = app.practice[w.set];

    if (w.runPhase == 2) do_run(app);

    // Exam mode: time's up submits whatever is there
    if (w.examMode && !w.timeUp && !w.accepted && GetTime() >= w.endsAt) {
        w.timeUp = true;
        start_run(app, true);
    }

    bool cmd = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool locked = w.timeUp;
    if (IsKeyPressed(KEY_ESCAPE)) { leave(app); return; }
    if (!locked && ((cmd && IsKeyPressed(KEY_ENTER)) || IsKeyPressed(KEY_F5) || IsKeyPressed(KEY_F6)))
        start_run(app, shift || IsKeyPressed(KEY_F6));

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
        std::string t = clock_text(GetTime() - w.started);
        float tw = ui_text_width(t, 15, FontKind::Mono);
        ui_text(t, clockX - tw, 24, 15, COL_MUTED, FontKind::Mono);
    }

    // ----- Layout -----
    float top = HEADER_H + 16, bottom = SCREEN_H - FOOTER_H - 12;
    Rectangle desc = { MARGIN, top, 430, bottom - top };
    float rx = desc.x + desc.width + 16, rw = SCREEN_W - MARGIN - rx;
    Rectangle editorBox = { rx, top, rw, 342 };
    Rectangle console = { rx, editorBox.y + editorBox.height + 12, rw, bottom - (editorBox.y + editorBox.height + 12) };

    draw_description(app, desc);

    // The editor, with a bar above it
    ui_panel(editorBox, COL_SURFACE, COL_LINE, 1);
    ui_text("solution.cpp", editorBox.x + 18, editorBox.y + 11, 13, COL_TEXT, FontKind::Mono);
    std::string lang = "C / C++";
    ui_text(lang, editorBox.x + 18 + ui_text_width("solution.cpp", 13, FontKind::Mono) + 16, editorBox.y + 11, 13, COL_MUTED, FontKind::Mono);
    if (ui_button("Reset code", { editorBox.x + editorBox.width - 12 - 110, editorBox.y + 5, 110, 28 }, locked ? ButtonStyle::Disabled : ButtonStyle::Outline, COL_TEXT_SOFT)) {
        std::string starter;
        for (const std::string &line : p.starter) starter += line + "\n";
        eb_replace_all(w.code, starter);   // Ctrl+Z brings your code back
    }
    Rectangle editArea = { editorBox.x + 1, editorBox.y + 38, editorBox.width - 2, editorBox.height - 39 };
    editor_update_draw(w.code, w.view, editArea, w.runPhase == 0, locked);

    draw_console(app, console);

    // ----- Footer -----
    ui_footer_hints({ "Ctrl/Cmd+Enter run", "Ctrl/Cmd+Shift+Enter submit", "ESC problems" });
    float by = SCREEN_H - FOOTER_H + 8;
    ButtonStyle runStyle = (locked || w.runPhase != 0) ? ButtonStyle::Disabled : ButtonStyle::Outline;
    ButtonStyle submitStyle = (locked || w.runPhase != 0) ? ButtonStyle::Disabled : ButtonStyle::Primary;
    if (ui_button("Submit", { SCREEN_W - MARGIN - 120, by, 120, 40 }, submitStyle)) start_run(app, true);
    if (ui_button("Run", { SCREEN_W - MARGIN - 120 - 12 - 100, by, 100, 40 }, runStyle, COL_TEXT)) start_run(app, false);
    if (w.accepted) {
        std::string msg = w.examMode ? "Solved in " + clock_text(w.stoppedAt - w.started) : "Solved";
        float mw = ui_text_width(msg, 15, FontKind::UiBold);
        ui_text(msg, SCREEN_W - MARGIN - 120 - 12 - 100 - 20 - mw, by + 11, 15, COL_GREEN, FontKind::UiBold);
    } else if (w.timeUp) {
        std::string msg = "Time's up: your code was submitted";
        float mw = ui_text_width(msg, 14);
        ui_text(msg, SCREEN_W - MARGIN - 120 - 12 - 100 - 20 - mw, by + 12, 14, COL_CORAL);
    }

    // "Running" shows for one frame before the (blocking) run starts
    if (w.runPhase == 1) w.runPhase = 2;
}
