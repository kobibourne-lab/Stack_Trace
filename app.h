// app.h - everything the screens share, and the functions that move between them.

#pragma once
#include <string>
#include <vector>
#include "content.h"
#include "quiz.h"
#include "progress.h"
#include "practice.h"
#include "editbuf.h"
#include "editor.h"
#include "net.h"
#include "versus.h"

enum class Screen { Title, Map, Lesson, Quiz, ExamPrep, Problem, Versus, Profile };

// The problem being solved (the LeetCode-style screen).
struct Workspace {
    int set = 0, problem = 0;          // indexes into App::practice
    EditBuffer code;
    EditorView view;
    std::string draftPath;             // where the code is saved as you type ("" in exam mode)
    double lastSaved = 0;

    bool examMode = false;             // a timer, no hints, and no saved draft
    bool versus = false;               // part of a Versus match (screen_versus.cpp runs it)
    double started = 0;                // GetTime() when it was opened
    double endsAt = 0;                 // exam mode: when time runs out
    bool timeUp = false;
    double stoppedAt = 0;              // exam mode: when it was accepted (the clock stops)

    int runPhase = 0;                  // 0 idle, 1 showing "Running", 2 running now
    bool submitting = false;           // Submit (counts if accepted) rather than Run
    bool hasResult = false;
    RunResult result;
    bool resultWasSubmit = false;
    bool accepted = false;             // the last submit was accepted
    int selectedTest = 0;              // the test whose details are shown
    int runCount = 0;                  // goes up after every run (Versus watches it)

    int hintsShown = 0;
    float descScroll = 0, descHeight = 0;
    float consoleScroll = 0;
    float detailScroll = 0, detailHeight = 0;
};

// Versus mode: the connection, the match, and the lobby's text boxes.
struct VersusState {
    NetConn net;
    VsMatch match;
    std::string myName;
    std::string joinAddress;        // what's typed in the Join box
    int focus = 0;                  // the text box being typed in: 0 none, 1 name, 2 address
    std::string localAddress;       // this computer's address, shown when hosting
    std::string message;            // an error or notice in the lobby
    bool helloSent = false;
    int lastRunCount = 0;           // the workspace's runCount when last checked
    int lastLines = -1;             // the line count last sent
    double lastLinesSent = 0;
    bool timeUpHandled = false;
    bool codeSent = false;
};

struct App {
    Screen screen = Screen::Title;   // the start screen comes first
    bool quit = false;

    std::vector<Topic> topics;
    int selected = 0;            // the topic picked on the map

    Progress progress;
    std::string progressPath = "progress.txt";

    Lesson lesson;               // the open lesson
    int step = 0;                // which of its steps is showing (0 = first)

    QuizSession quiz;            // the quiz in progress

    std::vector<ProblemSet> practice;   // Exam prep problems, from content/practice/
    std::string practiceError;          // problems that didn't load, shown on Exam prep
    int examSet = 0;             // the tab picked on the Exam prep screen
    int examSelected = 0;        // the problem picked on the Exam prep screen
    float examScroll = 0;        // how far the problem list is scrolled
    std::string examMessage;     // shown on its card

    Workspace work;              // the open problem
    VersusState vs;              // Versus mode

    std::string message;         // shown on the map, e.g. a file that wouldn't load
};

// Moving between screens. open_lesson and open_quiz load the files for the
// selected topic; if that fails they stay on the map and set app.message.
void open_map(App &app);
void open_lesson(App &app);
void open_quiz(App &app);
void open_exam_prep(App &app);
// Opens a problem on the problem screen. examMode starts the timer and hides hints.
void open_problem(App &app, int set, int problem, bool examMode);

// Records an answer for the topic and saves progress.txt straight away.
void record_answer(App &app, const std::string &topic, bool correct);

// One function per screen: handles its input and draws it, once per frame.
void screen_title(App &app);
void screen_map(App &app);
void screen_lesson(App &app);
void screen_quiz(App &app);
void screen_exam(App &app);
void screen_problem(App &app);
void save_draft(App &app);           // saves the open problem's code (main.cpp calls it on quit too)
void open_versus(App &app);
void screen_versus(App &app);
void versus_quit(App &app);          // tells the other player you've gone (main.cpp calls it on quit)

// Pieces of the problem screen that Versus reuses (screen_problem.cpp)
void problem_start_run(App &app, bool submit);
void problem_run_pending(App &app);
void problem_end_frame(App &app);
void problem_draw_description(App &app, Rectangle r);
void problem_draw_editor(App &app, Rectangle box, bool locked);
void problem_draw_console(App &app, Rectangle r);
void screen_profile(App &app);
