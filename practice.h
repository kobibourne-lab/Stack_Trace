// practice.h - Exam prep problems, LeetCode style: load them from
// content/practice/, check the rules (like "no []"), and test an answer by
// compiling it with the tests and running it.
//
// No drawing in here (no raylib), so tests/test_core.cpp can test it.
// The problem file format is described in README.md under "Writing problems".

#pragma once
#include <mutex>
#include <string>
#include <vector>

// A rule the code is checked for, e.g. no [ for "pointer arithmetic only".
struct Forbid {
    std::string token;     // "[", "if", "<<" ... (words match whole words only)
    std::string message;   // shown when it's broken
};

struct Problem {
    std::string id;          // "lab1/q01_leap_year": the set and the file name
    std::string set;         // "lab1"
    std::string title;       // "Leap Year"
    std::string function;    // "isLeapYear": the name shown in the list
    std::string group;       // "Numbers": the list's headings
    std::string difficulty;  // "Easy", "Medium" or "Hard"
    std::string topic;       // topic id on the map that solving it counts towards ("" for none)
    std::string prototype;   // shown on the card
    std::vector<std::string> text;     // paragraphs of the description
    std::vector<std::string> rules;    // shown as a list ("Use exactly 2 for loops")
    std::vector<Forbid> forbids;       // rules the code is checked for
    std::vector<std::string> hints;    // revealed one at a time (not in exam mode)
    std::vector<std::string> example;  // code lines
    std::vector<std::string> starter;  // the code the editor starts with
    std::vector<std::string> tests;    // the body of the test function
    int examMinutes = 15;              // exam mode's time limit
};

struct ProblemSet {
    std::string id;      // "lab1"
    std::string title;   // "Lab 1"
    std::string exam;    // "Exam 1", "Exam 2" or ""
    bool campaignOnly = false;   // only fought as campaign bosses: not in Exam prep or Versus
    std::vector<Problem> problems;
};

// Loads content/practice/problems.txt and every problem it lists. Problems
// that fail to load are skipped and named in error; returns false only if
// nothing at all could be loaded.
// Campaign-only sets ("SET: boss | Boss fights | campaign") are put last.
bool load_problem_sets(const std::string &dir, std::vector<ProblemSet> &sets, std::string &error);

// How many sets Exam prep and Versus show: all of them except the
// campaign-only ones, which load_problem_sets puts at the end.
int shown_set_count(const std::vector<ProblemSet> &sets);
bool parse_problem(const std::string &text, Problem &p, std::string &error);

bool forbids_token(const Problem &p, const std::string &token);   // e.g. "[" for the "no []" tag

// ---------------------------------------------------------------------------
// Checking an answer
// ---------------------------------------------------------------------------

// The code with comments, strings and character literals blanked out, so a
// "[" inside a comment doesn't count as breaking the "no []" rule.
std::string strip_comments_and_strings(const std::string &code);

// The messages of every forbidden token found in the code (empty: all fine).
std::vector<std::string> check_rules(const Problem &p, const std::string &code);

// The whole test program: a few helpers, the student's code, then the tests.
std::string build_test_program(const Problem &p, const std::string &code, int timeLimitSeconds);

enum class TestOutcome {
    Passed, Failed,
    Crashed,       // the code crashed during this test (crash says how)
    TimedOut,      // this test never finished (an endless loop?)
    NotRun         // couldn't run: the code before it crashed or hung every time
};

struct TestResult {
    bool passed = false;
    TestOutcome outcome = TestOutcome::Failed;
    std::string description;
    std::string expected;   // may contain '\n' (patterns, grids)
    std::string got;
    std::string crash;      // Crashed: "segmentation fault", "divide by zero" ...
    std::string code;       // the test's own lines: what it was doing (shown when it crashes)
};

// One check in a problem's tests, found by reading them (before running):
// its description, and its lines plus the lines just before it that set it up.
struct CheckInfo {
    std::string description;
    std::string code;
};
std::vector<CheckInfo> split_checks(const Problem &p);

enum class RunStatus {
    Passed,        // every test passed
    Failed,        // it ran, some tests failed
    CompileError,  // it didn't compile: see compilerOutput
    Crashed,       // it stopped part way (e.g. a segmentation fault)
    TimedOut,      // it ran past the time limit (usually an endless loop)
    NoCompiler     // no C++ compiler could be found
};

// A problem the compiler found, for the Problems list and the editor.
struct CompileProblem {
    int line = 0, col = 0;      // in the student's code (1-based); 0 = not in it
    bool error = true;          // false: a warning
    std::string message;
    bool inTests = false;       // it's in the tests: the function doesn't match the prototype
};
std::vector<CompileProblem> parse_compile_problems(const std::string &compilerOutput);

// What a run is doing right now, for the screen to show while it runs on
// another thread. Lock `lock` to read or change it.
struct RunProgress {
    std::mutex lock;
    std::vector<std::string> log;   // the terminal: what's happened, a line each
    std::string phase;              // "Compiling", "Running test 3 of 7"
    int total = 0;                  // checks in the problem
    std::vector<int> state;         // per test: 0 waiting, 1 passed, 2 failed, 3 crashed, 4 timed out, 5 not run
    bool done = false;
};

struct RunResult {
    RunStatus status = RunStatus::Failed;
    std::vector<TestResult> tests;
    int passed = 0, failed = 0;
    std::vector<std::string> ruleProblems;  // broken rules (an answer with these isn't accepted)
    std::string compilerOutput;             // errors and warnings, with the student's line numbers
    std::string programOutput;              // anything the student's code printed
    double seconds = 0;                     // how long compiling and running took
    std::vector<CompileProblem> problems;   // CompileError: where the errors are
    std::vector<std::string> log;           // the terminal's lines from the run
    int crashedAt = -1;                     // the first test that crashed or hung (-1: none)
};

// Reads what the test program printed (also used by the tests).
void parse_test_output(const std::string &output, RunResult &r);

// Accepted = it ran, every test passed and no rules were broken.
bool run_accepted(const RunResult &r);

// Compiles and runs the answer. Blocks until done, so the screens call it on
// another thread and watch `live` (optional) to show progress as it goes.
// workDir holds the generated files (made if missing).
//
// Each test has its own time limit (TEST_SECONDS). When the code crashes or
// hangs in a test, that test is marked and the program is run again from the
// next one, so one bad test doesn't hide the rest.
const int TEST_SECONDS = 3;
RunResult run_answer(const Problem &p, const std::string &code, const std::string &workDir, RunProgress *live = nullptr);

// Bytes for solving a problem in exam mode: the first solve pays 10 / 20 / 30
// (Easy / Medium / Hard), beating your best time pays half that, else nothing.
int exam_reward(const std::string &difficulty, bool firstSolve, bool newBest);

// A plain explanation of a crash ("segmentation fault": "your code used a
// pointer to nothing ...").
std::string crash_explanation(const std::string &crash);

// The C++ compiler command found on this computer ("" if none), searched for
// once: STACKTRACE_CXX if set, then c++ / g++ / clang++, then raylib's
// w64devkit on Windows.
const std::string &find_compiler();

// MAL-OS's gcc: compiles a plain C program (as C, not C++) to workDir/a.out
// (a.exe on Windows), with a watchdog that stops it after 3 seconds so an
// endless loop can't freeze the app. Returns false on a compile error, with
// the compiler's messages in output. cpp compiles it as C++ instead (hello.cpp),
// for the C Codex's C++ entries.
bool compile_c_program(const std::string &code, const std::string &workDir, std::string &output, bool cpp = false);

// Runs what compile_c_program made, with no keyboard input, and returns what
// it printed.
std::string run_c_program(const std::string &workDir);

// Where a student's draft for a problem is saved, e.g. saves/lab1_q01_leap_year.cpp
std::string draft_path(const std::string &savesDir, const Problem &p);
