// practice.h - Exam prep problems, LeetCode style: load them from
// content/practice/, check the rules (like "no []"), and test an answer by
// compiling it with the tests and running it.
//
// No drawing in here (no raylib), so tests/test_core.cpp can test it.
// The problem file format is described in README.md under "Writing problems".

#pragma once
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
    std::vector<Problem> problems;
};

// Loads content/practice/problems.txt and every problem it lists. Problems
// that fail to load are skipped and named in error; returns false only if
// nothing at all could be loaded.
bool load_problem_sets(const std::string &dir, std::vector<ProblemSet> &sets, std::string &error);
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

struct TestResult {
    bool passed = false;
    std::string description;
    std::string expected;   // may contain '\n' (patterns, grids)
    std::string got;
};

enum class RunStatus {
    Passed,        // every test passed
    Failed,        // it ran, some tests failed
    CompileError,  // it didn't compile: see compilerOutput
    Crashed,       // it stopped part way (e.g. a segmentation fault)
    TimedOut,      // it ran past the time limit (usually an endless loop)
    NoCompiler     // no C++ compiler could be found
};

struct RunResult {
    RunStatus status = RunStatus::Failed;
    std::vector<TestResult> tests;
    int passed = 0, failed = 0;
    std::vector<std::string> ruleProblems;  // broken rules (an answer with these isn't accepted)
    std::string compilerOutput;             // errors and warnings, with the student's line numbers
    std::string programOutput;              // anything the student's code printed
    double seconds = 0;                     // how long compiling and running took
};

// Reads what the test program printed (also used by the tests).
void parse_test_output(const std::string &output, RunResult &r);

// Accepted = it ran, every test passed and no rules were broken.
bool run_accepted(const RunResult &r);

// Compiles and runs the answer. Blocks until done (about a second); the
// screen shows "Running" first so the window doesn't look frozen.
// workDir holds the generated files (made if missing).
RunResult run_answer(const Problem &p, const std::string &code, const std::string &workDir);

// The C++ compiler command found on this computer ("" if none), searched for
// once: STACKTRACE_CXX if set, then c++ / g++ / clang++, then raylib's
// w64devkit on Windows.
const std::string &find_compiler();

// Where a student's draft for a problem is saved, e.g. saves/lab1_q01_leap_year.cpp
std::string draft_path(const std::string &savesDir, const Problem &p);
