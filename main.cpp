// Stack Trace - a C and C++ learning app, written in C++ with raylib.
//
// Files:
//   main.cpp            window, main loop, and moving between screens
//   app.h               what the screens share (the App struct)
//   content.cpp/.h      loading topics, lessons and quizzes from content/   (no drawing)
//   quiz.cpp/.h         score and lives for a quiz                         (no drawing)
//   progress.cpp/.h     mastery points and the answer log, in progress.txt (no drawing)
//   ui.cpp/.h           shared drawing: text, panels, buttons, code, header
//   memview.cpp/.h      the memory view: stack frames, heap blocks, pointer arrows
//   theme.h             colours and sizes
//   screen_map.cpp      the topic map
//   screen_lesson.cpp   a lesson: tutorial text, code, and the memory view step by step
//   screen_quiz.cpp     a quiz, then its results
//   practice.cpp/.h     Exam prep problems: loading, rule checks, compiling and testing answers (no drawing)
//   editbuf.cpp/.h      the code editor's text and edits                   (no drawing)
//   editor.cpp/.h       the code editor: drawing, colours, keys
//   screen_exam.cpp     Exam prep: the problem list, NeetCode style
//   screen_problem.cpp  one problem, LeetCode style: description, editor, test results
//   screen_profile.cpp  the profile: stats, badges, pinned topics, activity heatmap
//
// The files marked "no drawing" never call raylib, so tests/test_core.cpp can
// test them without a window: make test.

#include "raylib.h"
#include "theme.h"
#include "ui.h"
#include "app.h"
#include <fstream>
#include <sstream>

// ---------------------------------------------------------------------------
// Moving between screens
// ---------------------------------------------------------------------------

void open_map(App &app)
{
    app.screen = Screen::Map;
}

void open_lesson(App &app)
{
    const Topic &t = app.topics[app.selected];
    if (t.lessonFile.empty()) {
        app.message = "There's no lesson for " + t.title + " yet.";
        return;
    }
    std::string error;
    if (!load_lesson("content/" + t.lessonFile, app.lesson, error)) {
        app.message = error;
        return;
    }
    app.step = 0;
    app.message.clear();
    app.screen = Screen::Lesson;
}

void open_quiz(App &app)
{
    const Topic &t = app.topics[app.selected];
    if (t.quizFile.empty()) {
        app.message = "There's no quiz for " + t.title + " yet.";
        return;
    }
    Quiz quiz;
    std::string error;
    if (!load_quiz("content/" + t.quizFile, quiz, error)) {
        app.message = error;
        return;
    }
    quiz_start(app.quiz, quiz);
    app.message.clear();
    app.screen = Screen::Quiz;
}

void open_exam_prep(App &app)
{
    app.screen = Screen::ExamPrep;
}

void open_problem(App &app, int set, int problem, bool examMode)
{
    if (set < 0 || set >= (int)app.practice.size()) return;
    if (problem < 0 || problem >= (int)app.practice[set].problems.size()) return;
    const Problem &p = app.practice[set].problems[problem];

    Workspace &w = app.work;
    w = Workspace();
    w.set = set;
    w.problem = problem;
    w.examMode = examMode;

    // Start from the saved draft if there is one (exam mode always starts fresh)
    std::string text;
    for (const std::string &line : p.starter) text += line + "\n";
    if (!examMode) {
        w.draftPath = draft_path("saves", p);
        std::ifstream f(w.draftPath);
        if (f) {
            std::stringstream ss;
            ss << f.rdbuf();
            if (!ss.str().empty()) text = ss.str();
        }
    }
    eb_set_text(w.code, text);

    // Put the cursor at the end of the "// your code here" line, ready to type
    for (int i = 0; i < (int)w.code.lines.size(); i++) {
        if (w.code.lines[i].find("// your code here") != std::string::npos) {
            eb_move_to(w.code, i, (int)w.code.lines[i].size());
            break;
        }
    }

    w.started = GetTime();
    w.endsAt = w.started + p.examMinutes * 60.0;
    app.examMessage.clear();
    app.screen = Screen::Problem;
}

void record_answer(App &app, const std::string &topic, bool correct)
{
    progress_record(app.progress, topic, correct, today_string());
    std::string error;
    if (!progress_save(app.progressPath, app.progress, error))
        TraceLog(LOG_WARNING, "PROGRESS: %s", error.c_str());
}

// ---------------------------------------------------------------------------

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "Stack Trace - learn C and C++ by watching memory");
    SetExitKey(KEY_NULL);   // ESC goes back a screen instead of closing the window
    SetTargetFPS(60);
    ui_load_fonts();

    App app;
    std::string error;
    if (!load_topics("content/topics.txt", app.topics, error)) {
        // Without topics there's nothing to show: say why, and how to fix it.
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(COL_BG);
            ui_text("Couldn't load content/topics.txt", MARGIN, 80, 24, COL_CORAL, FontKind::UiBold);
            ui_text(error, MARGIN, 120, 16, COL_TEXT_SOFT, FontKind::Mono);
            ui_text("Run Stack Trace from the project folder (make run does this).", MARGIN, 150, 16, COL_MUTED);
            EndDrawing();
        }
        ui_unload_fonts();
        CloseWindow();
        return 1;
    }
    if (!load_problem_sets("content/practice", app.practice, app.practiceError))
        TraceLog(LOG_WARNING, "PRACTICE: %s", app.practiceError.c_str());
    if (!progress_load(app.progressPath, app.progress, error))
        TraceLog(LOG_WARNING, "PROGRESS: %s", error.c_str());

    while (!WindowShouldClose() && !app.quit) {
        BeginDrawing();
        ClearBackground(COL_BG);
        ui_begin_frame();

        switch (app.screen) {
            case Screen::Map:      screen_map(app); break;
            case Screen::Lesson:   screen_lesson(app); break;
            case Screen::Quiz:     screen_quiz(app); break;
            case Screen::ExamPrep: screen_exam(app); break;
            case Screen::Problem:  screen_problem(app); break;
            case Screen::Profile:  screen_profile(app); break;
        }

        ui_end_frame();
        EndDrawing();
    }

    if (app.screen == Screen::Problem) save_draft(app);
    ui_unload_fonts();
    CloseWindow();
    return 0;
}
