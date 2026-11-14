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
#include "mos.h"
#include "wardrobe.h"
#include "daily.h"
#include "outputle.h"
#include "codex.h"
#include "bugdex.h"
#include "farm.h"

enum class Screen { Title, Map, Lesson, Quiz, ExamPrep, Problem, Versus, Profile, Campaign, Boss, Room, Game, Watch, Daily, Outputle, Codex, Bugdex, Farm };

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
    double stoppedAt = 0;              // when it was accepted (the clock stops)

    // The "Solved!" card shown when a submit is accepted
    bool finishShown = false;          // showing it now (closed with "Keep my code open")
    double finishAt = 0;
    bool firstSolve = false;           // the first time this problem was solved (XP!)
    bool newBest = false;              // faster than the last time
    int solveSeconds = 0, bestSeconds = 0;

    int runPhase = 0;                  // 0 idle, 1 showing "Running", 2 running now
    bool submitting = false;           // Submit (counts if accepted) rather than Run
    bool hasResult = false;
    RunResult result;
    bool resultWasSubmit = false;
    bool accepted = false;             // the last submit was accepted
    int selectedTest = 0;              // the test whose details are shown
    int runCount = 0;                  // goes up after every run (Versus watches it)
    bool hadCrash = false, hadTimeout = false;   // for the Bugdex: fixing these catches a Segfault / Infinite Loop

    int hintsShown = 0;
    float descScroll = 0, descHeight = 0;
    float consoleScroll = 0;
    float detailScroll = 0, detailHeight = 0;
};

// Versus mode: the connection, the match, and the lobby's text boxes.
// A spectator connected to this computer while it hosts (screen_versus.cpp).
struct WatcherConn {
    NetConn conn;
    VsWatcher who;                  // their name and prediction
    bool hello = false;             // they've said WATCH
};

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

    // Spectators (only while hosting)
    NetServer watchServer;
    std::vector<WatcherConn> watchers;
    bool predictionsOpen = true;
    VsPhase lastPhase = VsPhase::Lobby;
    std::string lastLive, theirLive;  // the latest code from each player, for spectators who arrive late
    double lastLiveSent = 0;
    int theirLevel = 0, theirSolved = 0;
    int watchersShown = -1;           // the count last told to the joiner
    int watcherCount = 0;             // as the joiner: how many are watching (from WATCHERS)

    // Mals: the opponent's (dressed by their LOOK), and the watchers' (the host's lobby)
    Mal theirMal;
    std::vector<Mal> crowd;
    int theirRunsSeen = 0;            // so their Mal reacts once to each run
    bool theirDoneSeen = false;
};

// Watching someone else's match (screen_watch.cpp).
struct WatchState {
    NetConn net;
    VsSpectate spec;
    std::string address;
    bool helloSent = false;
    int pick = -1;                  // 0 the host, 1 the joiner
    int amount = 25;
    bool locked = false;            // the prediction has been sent (its bytes are taken off)
    bool paid = false;              // the result has been paid out
    int lockedBytes = 0, lockedSide = -1;
    int payout = 0;                 // what came back
    std::string message;

    // Mals: both players', and the other spectators' in the crowd
    Mal mals[2];
    std::vector<Mal> crowd;
    int runsSeen[2] = { 0, 0 }, bestSeen[2] = { 0, 0 };
    bool doneSeen[2] = { false, false };
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
    WatchState watch;            // watching a Versus match

    std::vector<Region> campaign;       // content/campaign.txt
    std::string campaignError;
    bool inCampaign = false;     // lessons and quizzes go back to the campaign map, not the topic map
    int campaignRegion = 0;      // the stop picked on the campaign map: its region...
    int campaignTopic = 0;       // ...and its topic in that region (-1 = the boss)

    Mos mos;                     // MAL-OS, the computer in Mal's room (kept while you play its games)
    std::string gameId;          // the MAL-OS minigame being played
    BossState boss;

    std::string message;         // shown on the map, e.g. a file that wouldn't load

    Mal mal;                     // the mascot, shared by every screen
    int levelShown = 1;          // the level last celebrated, so each level-up shows once
    float levelUpLeft = 0;       // seconds left on the level-up banner

    // The daily loop (screen_daily.cpp)
    Screen dailyFrom = Screen::Title;   // where Back goes from the Daily screen
    float streakLeft = 0;        // seconds left on the streak banner (waits for the level-up one)
    int freezesUsed = 0;         // const freezes used when the app opened (the banner says so)
    float toastLeft = 0;         // a quest just finished (or a bug was caught): the toast in the corner
    std::string toastTitle, toastText, toastSub;
    bool toastBug = false;       // the toast is for a bug caught (draws the bug, not a chest)
    std::string toastBugId;

    Screen returnTo = Screen::Title;    // where Outputle, the Codex, the Bugdex and the farm go back to
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

// Save progress.txt now (record_answer does this after every answer).
void save_progress(App &app);

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
void open_watch(App &app, const std::string &address);   // watch the match hosted at address
void screen_watch(App &app);
void watch_quit(App &app);                                // at shutdown
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

// Mal's room (screen_room.cpp): the room, MAL-OS on its computer, the wardrobe
// and the code tree. The minigames are in screen_games.cpp.
void open_room(App &app);
void screen_room(App &app);
void start_game(App &app, const std::string &id);
void screen_games(App &app);

// What's unlocked for the wardrobe and MAL-OS, from progress.
UnlockInfo unlock_info(const App &app);
MosInfo mos_info(const App &app);

// Put on a wardrobe item (if unlocked) and save it.
void wear_item(App &app, const WardrobeItem &item);
void screen_campaign(App &app);

// The daily loop (screen_daily.cpp): the streak, const freezes and quests.
void open_daily(App &app);
void screen_daily(App &app);
// Something was learned today (an answer, a lesson, a solve, a boss): keeps
// the streak alive, and shows the flame banner the first time each day.
void learned_today(App &app);
// A quest counter went up ("correct", "game"...), or a best of the day
// ("combo", "snake"...). Saves, and shows a toast if it finished a quest.
void quest_event(App &app, const std::string &key, int n = 1);
void quest_best(App &app, const std::string &key, int value);
// The streak banner and the quest toast, over every screen (main.cpp).
void draw_daily_overlays(App &app);
// A bug caught (a boss beaten, a crash fixed...): adds it to the Bugdex,
// saves, and shows a toast. how: "boss", "exam prep", "practice".
void catch_bug(App &app, const std::string &bugId, const std::string &how);

// Outputle, the C Codex, the Bugdex and the Memory Farm
void open_outputle(App &app);
void screen_outputle(App &app);
void open_codex(App &app);
void screen_codex(App &app);
void open_bugdex(App &app);
void screen_bugdex(App &app);
void open_farm(App &app);
void screen_farm(App &app);
void draw_bug(const Bug &bug, Vector2 centre, float size, float t, bool silhouette);
// The code tree (a branch per region, a leaf per topic): Mal's room and the farm draw it.
void draw_code_tree(const App &app, Rectangle area, float t, bool labels);

// Drawing pieces the start screen shares.
void draw_flame(Vector2 base, float height, float t, bool lit);
void draw_freeze(Rectangle r, bool held, float t);
void draw_chest(Vector2 base, float width, int state, float t, bool golden, float openAge);   // state: 0 locked, 1 ready, 2 open
void open_boss(App &app, int region);   // the boss's intro; the fight starts from there
void screen_boss(App &app);
