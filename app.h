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
#include "mascot.h"
#include "campaign.h"

enum class Screen { Title, Map, Lesson, Quiz, ExamPrep, Problem, Versus, Profile, Campaign, Boss };

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

// A campaign boss fight (screen_boss.cpp). The problem itself is in App::work.
struct BossState {
    int region = 0;                 // index into App::campaign
    int phase = 0;                  // 0 intro, 1 fight, 2 won, 3 lost
    bool found = false;             // the boss's problem is in content/practice
    int totalTests = 0;             // the boss's HP when the fight starts
    int bestPassed = 0;             // the most tests any run has passed (the hits so far)
    int lives = 3, livesLost = 0;
    int lastRunCount = 0;           // the workspace's runCount when last checked
    bool timeUp = false, timeUpSubmitted = false;
    std::string banner;             // what the last run did ("Hit! -2 HP"), over the editor
    bool bannerGood = false;
    double bannerAt = -100, hitAt = -100, hurtAt = -100, escArmed = -100, phaseAt = 0;
    int xpBefore = 0, xpGained = 0;
    double seconds = 0;             // how long the win took
    int stars = 0;
    bool firstWin = false;
    std::string lostReason;
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

    std::vector<Region> campaign;       // content/campaign.txt
    std::string campaignError;
    bool inCampaign = false;     // lessons and quizzes go back to the campaign map, not the topic map
    int campaignRegion = 0;      // the stop picked on the campaign map: its region...
    int campaignTopic = 0;       // ...and its topic in that region (-1 = the boss)
    BossState boss;

    std::string message;         // shown on the map, e.g. a file that wouldn't load

    Mal mal;                     // the mascot, shared by every screen
    int levelShown = 1;          // the level last celebrated, so each level-up shows once
    float levelUpLeft = 0;       // seconds left on the level-up banner
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

// If the XP has reached a new level since the last check, show the level-up
// banner (with a fanfare). record_answer calls it; so does anything else that
// gives XP.
void check_level_up(App &app);

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
void open_campaign(App &app);
void screen_campaign(App &app);
void open_boss(App &app, int region);   // the boss's intro; the fight starts from there
void screen_boss(App &app);
