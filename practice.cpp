// practice.cpp - loading problems and testing answers. See practice.h.
//
// How an answer is tested:
//   1. build_test_program() writes one .cpp file: some test helpers, the
//      student's code (with #line so compiler errors show their own line
//      numbers), then the problem's tests.
//   2. run_answer() compiles it with the computer's C++ compiler and runs it.
//   3. The test program prints one line per test, starting @@PASS or @@FAIL,
//      and parse_test_output() turns those back into results.
// Before each check it prints @@AT and the check's number, and restarts a
// watchdog that stops it if that one test takes more than TEST_SECONDS. A
// crash prints @@CRASH and what kind. Either way run_answer() marks that test
// and runs the program again from the next check (the program skips the
// checks before it), so every test still gets a result.

#include "practice.h"
#include "content.h"   // trim, split_fields
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <functional>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #define ST_POPEN _popen
    #define ST_PCLOSE _pclose
#else
    #include <sys/wait.h>
    #define ST_POPEN popen
    #define ST_PCLOSE pclose
#endif

// Stop rerunning after this long, and mark what's left "not run"
const double RUN_BUDGET_SECONDS = 25;

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

static std::vector<std::string> split_lines(const std::string &text)
{
    std::vector<std::string> lines;
    std::string line;
    std::stringstream ss(text);
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

static std::string untab(const std::string &line)
{
    std::string out;
    for (char c : line) {
        if (c == '\t') out += "    ";
        else out += c;
    }
    return out;
}

static bool read_file(const std::string &path, std::string &text)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    text = ss.str();
    return true;
}

bool parse_problem(const std::string &text, Problem &p, std::string &error)
{
    p = Problem();
    std::vector<std::string> *block = nullptr;   // the code block being read, if any

    for (const std::string &raw : split_lines(text)) {
        if (block != nullptr) {
            if (trim(raw) == "END") block = nullptr;
            else block->push_back(untab(raw));
            continue;
        }
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = trim(line.substr(0, colon));
        std::string value = trim(line.substr(colon + 1));

        if (key == "TITLE") p.title = value;
        else if (key == "FUNCTION") p.function = value;
        else if (key == "GROUP") p.group = value;
        else if (key == "DIFFICULTY") p.difficulty = value;
        else if (key == "TOPIC") p.topic = value;
        else if (key == "PROTOTYPE") p.prototype = value;
        else if (key == "TEXT") p.text.push_back(value);
        else if (key == "RULE") p.rules.push_back(value);
        else if (key == "HINT") p.hints.push_back(value);
        else if (key == "MINUTES") p.examMinutes = std::atoi(value.c_str()) > 0 ? std::atoi(value.c_str()) : p.examMinutes;
        else if (key == "FORBID") {
            std::vector<std::string> f = split_fields(value, '|');
            if (!f.empty() && !f[0].empty()) p.forbids.push_back({ f[0], f.size() > 1 ? f[1] : "Not allowed: " + f[0] });
        }
        else if (key == "EXAMPLE") block = &p.example;
        else if (key == "STARTER") block = &p.starter;
        else if (key == "TESTS") block = &p.tests;
    }

    if (p.difficulty.empty()) p.difficulty = "Medium";
    if (p.function.empty()) p.function = p.title;
    if (p.title.empty()) { error = "no TITLE"; return false; }
    if (p.starter.empty()) { error = "no STARTER code"; return false; }
    if (p.tests.empty()) { error = "no TESTS"; return false; }
    return true;
}

bool load_problem_sets(const std::string &dir, std::vector<ProblemSet> &sets, std::string &error)
{
    sets.clear();
    error.clear();
    std::string index;
    if (!read_file(dir + "/problems.txt", index)) {
        error = "could not open " + dir + "/problems.txt";
        return false;
    }

    for (const std::string &raw : split_lines(index)) {
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;

        if (line.compare(0, 4, "SET:") == 0) {
            // SET: id | title | exam (or "campaign" for boss problems only fought in the campaign)
            std::vector<std::string> f = split_fields(line.substr(4), '|');
            ProblemSet s;
            s.id = f.size() > 0 ? f[0] : "";
            s.title = f.size() > 1 ? f[1] : s.id;
            if (f.size() > 2 && f[2] == "campaign") s.campaignOnly = true;
            else if (f.size() > 2 && f[2] != "-") s.exam = f[2];
            sets.push_back(s);
            continue;
        }
        if (sets.empty()) continue;   // a file before any SET line

        std::string text, problemError;
        Problem p;
        if (!read_file(dir + "/" + line, text)) {
            error += "could not open " + line + ". ";
            continue;
        }
        if (!parse_problem(text, p, problemError)) {
            error += line + ": " + problemError + ". ";
            continue;
        }
        std::string stem = line;
        size_t dot = stem.rfind('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        p.id = stem;
        p.set = sets.back().id;
        sets.back().problems.push_back(p);
    }

    // Sets with nothing in them aren't worth showing. Campaign-only sets go last,
    // so Exam prep and Versus can just show the first shown_set_count() sets.
    std::vector<ProblemSet> kept;
    for (const ProblemSet &s : sets) if (!s.problems.empty() && !s.campaignOnly) kept.push_back(s);
    for (const ProblemSet &s : sets) if (!s.problems.empty() && s.campaignOnly) kept.push_back(s);
    sets = kept;
    if (sets.empty() && error.empty()) error = "no problems found";
    return !sets.empty();
}

int shown_set_count(const std::vector<ProblemSet> &sets)
{
    int n = 0;
    for (const ProblemSet &s : sets) if (!s.campaignOnly) n++;
    return n;
}

bool forbids_token(const Problem &p, const std::string &token)
{
    for (const Forbid &f : p.forbids) if (f.token == token) return true;
    return false;
}

std::string draft_path(const std::string &savesDir, const Problem &p)
{
    std::string name = p.id;
    for (char &c : name) if (c == '/' || c == '\\') c = '_';
    return savesDir + "/" + name + ".cpp";
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

std::string strip_comments_and_strings(const std::string &code)
{
    std::string out = code;
    size_t i = 0, n = code.size();
    while (i < n) {
        if (code.compare(i, 2, "//") == 0) {
            while (i < n && code[i] != '\n') out[i++] = ' ';
        } else if (code.compare(i, 2, "/*") == 0) {
            size_t end = code.find("*/", i + 2);
            end = (end == std::string::npos) ? n : end + 2;
            for (; i < end; i++) if (code[i] != '\n') out[i] = ' ';
        } else if (code[i] == '"' || code[i] == '\'') {
            char quote = code[i++];
            while (i < n && code[i] != quote && code[i] != '\n') {
                if (code[i] == '\\' && i + 1 < n) out[i++] = ' ';
                out[i++] = ' ';
            }
            if (i < n && code[i] == quote) i++;
        } else {
            i++;
        }
    }
    return out;
}

static bool is_word_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

// Is token in the code? Words ("if") must stand alone; symbols ("[") can be anywhere.
static bool contains_token(const std::string &code, const std::string &token)
{
    bool word = !token.empty() && is_word_char(token[0]);
    size_t pos = 0;
    while ((pos = code.find(token, pos)) != std::string::npos) {
        if (!word) return true;
        bool before = pos > 0 && is_word_char(code[pos - 1]);
        bool after = pos + token.size() < code.size() && is_word_char(code[pos + token.size()]);
        if (!before && !after) return true;
        pos += token.size();
    }
    return false;
}

std::vector<std::string> check_rules(const Problem &p, const std::string &code)
{
    std::vector<std::string> broken;
    std::string clean = strip_comments_and_strings(code);
    for (const Forbid &f : p.forbids)
        if (contains_token(clean, f.token)) broken.push_back(f.message);
    return broken;
}

// ---------------------------------------------------------------------------
// The test program
// ---------------------------------------------------------------------------

// Before the student's code. Every header a student is likely to include is
// included first; then printf, putchar and puts are swapped for versions that
// can capture output, so pattern questions ("print a triangle") can be checked.
static const char *PRELUDE = R"ST(// Made by Stack Trace to test your answer. Safe to delete.
#include <cstdio>
#include <stdio.h>
#include <cstdlib>
#include <stdlib.h>
#include <cstring>
#include <string.h>
#include <cctype>
#include <ctype.h>
#include <cmath>
#include <math.h>
#include <cstdarg>
#include <stdbool.h>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>

static std::string st_capture;
static bool st_capturing = false;

static int st_printf(const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (st_capturing) { if (st_capture.size() < 100000) st_capture += buf; }
    else fputs(buf, stdout);
    return n;
}
static int st_putchar(int c)
{
    if (st_capturing) { if (st_capture.size() < 100000) st_capture += (char)c; }
    else fputc(c, stdout);
    return c;
}
static int st_puts(const char *s)
{
    if (st_capturing) { if (st_capture.size() < 100000) { st_capture += s; st_capture += '\n'; } }
    else { fputs(s, stdout); fputc('\n', stdout); }
    return 1;
}
#define printf st_printf
#define putchar st_putchar
#define puts st_puts
)ST";

// After the student's code: the check helpers the tests use.
static const char *HELPERS = R"ST(
#undef printf
#undef putchar
#undef puts

static int st_passed = 0, st_failed = 0;
static void st_tick();   // restarts the watchdog: each test gets its own time

// Results go out on one line each, so newlines and tabs inside them are swapped
static std::string st_line(std::string s)
{
    for (char &c : s) { if (c == '\n') c = '\x1f'; else if (c == '\t' || c == '\r') c = ' '; }
    return s;
}

static void st_result(bool ok, const std::string &desc, const std::string &expected, const std::string &got)
{
    std::cout.flush();
    fflush(stdout);
    if (ok) { st_passed++; printf("\n@@PASS\t%s\n", st_line(desc).c_str()); }
    else { st_failed++; printf("\n@@FAIL\t%s\t%s\t%s\n", st_line(desc).c_str(), st_line(expected).c_str(), st_line(got).c_str()); }
    fflush(stdout);
    st_tick();   // the setup for the next test gets a fresh time limit too
}

static void checkBool(bool got, bool expected, const char *d)
{
    st_result(got == expected, d, expected ? "true" : "false", got ? "true" : "false");
}

static void checkInt(int got, int expected, const char *d)
{
    st_result(got == expected, d, std::to_string(expected), std::to_string(got));
}

static std::string st_list(int n, const int *a)
{
    std::string s = "{";
    for (int i = 0; i < n; i++) { if (i > 0) s += ", "; s += std::to_string(a[i]); }
    return s + "}";
}

static void checkArray(int n, const int *got, const int *expected, const char *d)
{
    bool same = true;
    for (int i = 0; i < n; i++) if (got[i] != expected[i]) same = false;
    st_result(same, d, st_list(n, expected), st_list(n, got));
}

static void checkString(const char *got, const char *expected, const char *d)
{
    st_result(strcmp(got, expected) == 0, d, "\"" + std::string(expected) + "\"", "\"" + std::string(got) + "\"");
}

static void checkText(const std::string &got, const std::string &expected, const char *d)
{
    st_result(got == expected, d, expected, got);
}

static std::string st_grid(int g[4][6])
{
    std::string s;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 6; c++) {
            char cell[8];
            snprintf(cell, sizeof cell, "%3d", g[r][c]);
            s += cell;
        }
        if (r < 3) s += "\n";
    }
    return s;
}

static void checkGrid(int got[4][6], int expected[4][6], const char *d)
{
    bool same = true;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 6; c++)
            if (got[r][c] != expected[r][c]) same = false;
    st_result(same, d, st_grid(expected), st_grid(got));
}

// Runs f, capturing what it prints with printf, putchar, puts or cout.
template <typename F> static std::string st_capture_output(F f)
{
    st_capture.clear();
    st_capturing = true;
    std::stringstream ss;
    std::streambuf *old = std::cout.rdbuf(ss.rdbuf());
    f();
    std::cout.rdbuf(old);
    st_capturing = false;
    return st_capture + ss.str();
}

// Ignores spaces at the ends of lines, and blank lines at the very end.
static std::string st_tidy(const std::string &s)
{
    std::vector<std::string> lines;
    std::string line;
    std::stringstream ss(s);
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r')) line.pop_back();
        lines.push_back(line);
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    std::string out;
    for (size_t i = 0; i < lines.size(); i++) { if (i > 0) out += "\n"; out += lines[i]; }
    return out;
}

// The printed pattern must match exactly (apart from trailing spaces).
template <typename F> static void checkOutput(F f, const char *expected, const char *d)
{
    std::string got = st_capture_output(f);
    st_result(st_tidy(got) == st_tidy(expected), d, st_tidy(expected), st_tidy(got));
}

// The same numbers on each line, in columns that line up (any spacing).
template <typename F> static void checkOutputColumns(F f, const char *expected, const char *d)
{
    std::string got = st_tidy(st_capture_output(f));
    std::vector<std::vector<std::string>> gotWords, expWords;
    std::vector<std::vector<size_t>> starts, ends;
    std::stringstream gs(got), es(st_tidy(expected));
    std::string line;
    while (std::getline(gs, line)) {
        if (line.find_first_not_of(' ') == std::string::npos) continue;
        std::vector<std::string> words; std::vector<size_t> st, en;
        size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && line[i] == ' ') i++;
            if (i >= line.size()) break;
            size_t j = i;
            while (j < line.size() && line[j] != ' ') j++;
            words.push_back(line.substr(i, j - i)); st.push_back(i); en.push_back(j);
            i = j;
        }
        gotWords.push_back(words); starts.push_back(st); ends.push_back(en);
    }
    while (std::getline(es, line)) {
        std::stringstream ws(line); std::string w; std::vector<std::string> words;
        while (ws >> w) words.push_back(w);
        if (!words.empty()) expWords.push_back(words);
    }
    bool same = (gotWords == expWords);
    bool lined = true;
    if (same) {
        bool left = true, right = true;
        for (size_t r = 1; r < starts.size(); r++)
            for (size_t c = 0; c < starts[r].size() && c < starts[0].size(); c++) {
                if (starts[r][c] != starts[0][c]) left = false;
                if (ends[r][c] != ends[0][c]) right = false;
            }
        lined = left || right;
    }
    std::string why = same ? (lined ? "" : "\n(the numbers are right, but the columns don't line up)") : "";
    st_result(same && lined, d, st_tidy(expected), got + why);
}

)ST";

// Before the tests: every check goes through st_begin(), which says which
// check is next, restarts the watchdog, and skips the checks before the one
// to start from (without running their code: the arguments aren't evaluated).
static const char *CHECKS = R"ST(
static int st_at = -1, st_from = 0;
static bool st_begin()
{
    st_at++;
    printf("\n@@AT\t%d\n", st_at);
    fflush(stdout);
    st_tick();
    return st_at >= st_from;
}
#define checkBool(...) do { if (st_begin()) checkBool(__VA_ARGS__); } while (0)
#define checkInt(...) do { if (st_begin()) checkInt(__VA_ARGS__); } while (0)
#define checkArray(...) do { if (st_begin()) checkArray(__VA_ARGS__); } while (0)
#define checkString(...) do { if (st_begin()) checkString(__VA_ARGS__); } while (0)
#define checkText(...) do { if (st_begin()) checkText(__VA_ARGS__); } while (0)
#define checkGrid(...) do { if (st_begin()) checkGrid(__VA_ARGS__); } while (0)
#define checkOutput(...) do { if (st_begin()) checkOutput(__VA_ARGS__); } while (0)
#define checkOutputColumns(...) do { if (st_begin()) checkOutputColumns(__VA_ARGS__); } while (0)
)ST";

// The end: the watchdog (one test at a time), crash reports, then main().
static std::string footer(int seconds)
{
    std::string s = R"ST(
#include <exception>
#include <csignal>
static void st_say(const char *m);
static void st_crashed(const char *what)
{
    st_say("\n@@CRASH\t");
    st_say(what);
    st_say("\n");
}
static void st_on_terminate()
{
    st_crashed("uncaught exception");
    std::_Exit(134);
}
static void st_on_abort(int) { st_crashed("abort"); std::_Exit(134); }
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX   // (g++ 15 already defines it: defining it twice filled the errors box with warnings)
#endif
#include <windows.h>
static void st_say(const char *m)
{
    DWORD n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), m, (DWORD)strlen(m), &n, NULL);
}
static volatile ULONGLONG st_deadline = 0;
static void st_tick() { st_deadline = GetTickCount64() + ST_LIMIT * 1000; }
static DWORD WINAPI st_watchdog(LPVOID)
{
    for (;;) {
        Sleep(30);
        if (GetTickCount64() > st_deadline) {
            st_say("\n@@TIMEOUT\n");
            ExitProcess(124);
        }
    }
    return 0;
}
static LONG WINAPI st_on_exception(EXCEPTION_POINTERS *e)
{
    DWORD c = e->ExceptionRecord->ExceptionCode;
    st_crashed(c == EXCEPTION_ACCESS_VIOLATION ? "segmentation fault" :
               c == EXCEPTION_STACK_OVERFLOW ? "stack overflow" :
               c == EXCEPTION_INT_DIVIDE_BY_ZERO ? "divide by zero" :
               c == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ? "array out of bounds" :
               c == EXCEPTION_ILLEGAL_INSTRUCTION ? "illegal instruction" : "crash");
    ExitProcess(139);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void st_start_watchdog()
{
    st_tick();
    SetUnhandledExceptionFilter(st_on_exception);
    CreateThread(NULL, 0, st_watchdog, NULL, 0, NULL);
}
#else
#include <unistd.h>
static void st_say(const char *m) { if (write(1, m, strlen(m)) < 0) {} }
static void st_on_alarm(int) { st_say("\n@@TIMEOUT\n"); _exit(124); }
static void st_tick() { alarm(ST_LIMIT); }
static void st_on_signal(int sig)
{
    st_crashed(sig == SIGSEGV ? "segmentation fault" : sig == SIGBUS ? "bus error" :
               sig == SIGFPE ? "divide by zero" : sig == SIGILL ? "illegal instruction" : "crash");
    _exit(139);
}
static void st_start_watchdog()
{
    // The crash handler gets its own stack, so a stack overflow can still be reported
    static char altStack[65536];
    stack_t ss = {};
    ss.ss_sp = altStack;
    ss.ss_size = sizeof altStack;
    sigaltstack(&ss, NULL);
    struct sigaction sa = {};
    sa.sa_handler = st_on_signal;
    sa.sa_flags = SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    signal(SIGALRM, st_on_alarm);
    st_tick();
}
#endif

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);   // so nothing is lost if the code crashes
    if (argc > 1) st_from = atoi(argv[1]);   // start from this check (after a crash)
    std::set_terminate(st_on_terminate);
    signal(SIGABRT, st_on_abort);
    st_start_watchdog();
    st_tests();
    printf("\n@@SUMMARY\t%d\t%d\t%d\n", st_passed, st_failed, st_at + 1);
    return 0;
}
)ST";
    std::string limit = std::to_string(seconds);
    size_t pos;
    while ((pos = s.find("ST_LIMIT")) != std::string::npos) s.replace(pos, 8, limit);
    return s;
}

std::string build_test_program(const Problem &p, const std::string &code, int timeLimitSeconds)
{
    std::string out = PRELUDE;
    out += "#line 1 \"solution.cpp\"\n";
    out += code;
    if (!code.empty() && code.back() != '\n') out += "\n";
    out += "#line 1 \"stacktrace_helpers\"\n";
    out += HELPERS;
    out += CHECKS;
    out += "#line 1 \"tests\"\n";
    out += "static void st_tests()\n{\n";
    for (const std::string &line : p.tests) out += line + "\n";
    out += "}\n";
    out += "#line 1 \"stacktrace_main\"\n";
    out += footer(timeLimitSeconds);
    return out;
}

// ---------------------------------------------------------------------------
// Reading the results
// ---------------------------------------------------------------------------

static std::string unline(std::string s)
{
    for (char &c : s) if (c == '\x1f') c = '\n';
    return s;
}

void parse_test_output(const std::string &output, RunResult &r)
{
    r.tests.clear();
    r.passed = r.failed = 0;
    r.programOutput.clear();
    bool summary = false, timeout = false;

    for (const std::string &line : split_lines(output)) {
        if (line.compare(0, 7, "@@PASS\t") == 0) {
            TestResult t;
            t.passed = true;
            t.description = unline(line.substr(7));
            r.tests.push_back(t);
            r.passed++;
        } else if (line.compare(0, 7, "@@FAIL\t") == 0) {
            std::vector<std::string> f;
            std::string field;
            std::stringstream ss(line.substr(7));
            while (std::getline(ss, field, '\t')) f.push_back(field);
            TestResult t;
            t.description = f.size() > 0 ? unline(f[0]) : "";
            t.expected = f.size() > 1 ? unline(f[1]) : "";
            t.got = f.size() > 2 ? unline(f[2]) : "";
            r.tests.push_back(t);
            r.failed++;
        } else if (line.compare(0, 10, "@@SUMMARY\t") == 0) {
            summary = true;
        } else if (line == "@@TIMEOUT") {
            timeout = true;
        } else if (line.compare(0, 4, "@@AT") == 0 || line.compare(0, 7, "@@CRASH") == 0) {
            // which check is running, and how it crashed: run_answer() reads these
        } else if (!line.empty()) {
            r.programOutput += line + "\n";
        }
    }

    if (timeout) r.status = RunStatus::TimedOut;
    else if (!summary) r.status = RunStatus::Crashed;
    else r.status = (r.failed == 0) ? RunStatus::Passed : RunStatus::Failed;
}

bool run_accepted(const RunResult &r)
{
    return r.status == RunStatus::Passed && r.ruleProblems.empty() && !r.tests.empty();
}

// ---------------------------------------------------------------------------
// Compiling and running
// ---------------------------------------------------------------------------

// Runs a shell command, returning its output (stdout and stderr) and exit code.
// onLine (optional) sees each line as it comes, for a live view. A program
// killed by a signal returns 128 + the signal's number.
static int run_command(const std::string &command, std::string &output,
                       const std::function<void(const std::string &)> &onLine = nullptr)
{
    output.clear();
    std::string full = command + " 2>&1";
    FILE *pipe = ST_POPEN(full.c_str(), "r");
    if (pipe == nullptr) return -1;
    char buf[1024];
    std::string partial;
    while (fgets(buf, sizeof buf, pipe) != nullptr) {
        if (output.size() < 200000) output += buf;
        if (onLine) {
            partial += buf;
            if (!partial.empty() && partial.back() == '\n') {
                partial.pop_back();
                if (!partial.empty() && partial.back() == '\r') partial.pop_back();
                onLine(partial);
                partial.clear();
            } else if (partial.size() > 4000) {   // a very long line: pass it on in pieces
                onLine(partial);
                partial.clear();
            }
        }
    }
    if (onLine && !partial.empty()) onLine(partial);
    int status = ST_PCLOSE(pipe);
#ifndef _WIN32
    if (status != -1) status = WIFEXITED(status) ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 128);
#endif
    return status;
}

const std::string &find_compiler()
{
    static bool searched = false;
    static std::string found;
    if (searched) return found;
    searched = true;

    std::vector<std::string> candidates;
    if (const char *env = std::getenv("STACKTRACE_CXX")) candidates.push_back(env);
#ifdef _WIN32
    candidates.push_back("g++");
    candidates.push_back("C:/raylib/w64devkit/bin/g++.exe");
    candidates.push_back("c++");
#else
    candidates.push_back("c++");
    candidates.push_back("g++");
    candidates.push_back("clang++");
#endif
    for (std::string c : candidates) {
#ifdef _WIN32
        for (char &ch : c) if (ch == '/') ch = '\\';   // cmd.exe wants backslashes in the program's path
#endif
        std::string out;
        if (run_command(c + " --version", out) == 0) {
            found = c;
            break;
        }
    }
    return found;
}

// ---------------------------------------------------------------------------
// The tests' checks, read before running (for names and context of crashes)
// ---------------------------------------------------------------------------

static const char *CHECK_NAMES[] = { "checkBool", "checkInt", "checkArray", "checkString", "checkText",
                                     "checkGrid", "checkOutputColumns", "checkOutput" };

// How many checks start on this line (a call like checkInt( ...)
static int checks_on_line(const std::string &line)
{
    int n = 0;
    for (const char *name : CHECK_NAMES) {
        size_t len = strlen(name);
        for (size_t at = line.find(name); at != std::string::npos; at = line.find(name, at + len)) {
            bool startOfWord = at == 0 || !(isalnum((unsigned char)line[at - 1]) || line[at - 1] == '_');
            size_t end = at + len;
            if (end < line.size() && (isalnum((unsigned char)line[end]) || line[end] == '_')) continue;   // checkOutput vs checkOutputColumns
            while (end < line.size() && line[end] == ' ') end++;
            if (startOfWord && end < line.size() && line[end] == '(') n++;
        }
    }
    return n;
}

// The last "string literal" on the line: a check's description
static std::string last_string(const std::string &line)
{
    std::string last, cur;
    bool in = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (in) {
            if (c == '\\' && i + 1 < line.size()) { cur += line[++i]; continue; }
            if (c == '"') { in = false; last = cur; continue; }
            cur += c;
        } else if (c == '"') { in = true; cur.clear(); }
        else if (c == '\'' && i + 2 < line.size()) i += (line[i + 1] == '\\') ? 3 : 2;   // skip 'x' and '\n'
    }
    return last;
}

std::vector<CheckInfo> split_checks(const Problem &p)
{
    std::vector<CheckInfo> out;
    std::vector<std::string> setup;   // lines since the last check
    for (const std::string &raw : p.tests) {
        std::string line = trim(raw);
        int n = checks_on_line(line);
        if (n == 0) {
            if (!line.empty()) setup.push_back(line);
            continue;
        }
        // Only the few lines just before it (and none that are whole helper lambdas)
        std::string code;
        int start = (int)setup.size() - 3;
        if (start < 0) start = 0;
        for (int i = start; i < (int)setup.size(); i++) {
            const std::string &s = setup[i];
            if (s.find("auto ") == 0 || s == "};" || s.find("return ") == 0 || s.find("for (") == 0 || s == "}") continue;
            code += s + "\n";
        }
        code += line;
        for (int k = 0; k < n; k++) out.push_back({ last_string(line), code });
        setup.clear();
    }
    return out;
}

// ---------------------------------------------------------------------------
// Compiler messages and crashes, explained
// ---------------------------------------------------------------------------

std::vector<CompileProblem> parse_compile_problems(const std::string &compilerOutput)
{
    // Lines like  solution.cpp:12:5: error: expected ';' before '}' token
    std::vector<CompileProblem> out;
    std::stringstream ss(compilerOutput);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t kind = std::string::npos;
        bool error = true;
        for (const char *k : { ": error: ", ": fatal error: ", ": warning: " }) {
            size_t at = line.find(k);
            if (at != std::string::npos) { kind = at; error = std::string(k).find("warning") == std::string::npos; break; }
        }
        if (kind == std::string::npos) continue;
        CompileProblem cp;
        cp.error = error;
        cp.message = line.substr(line.find(':', kind + 2) + 2);
        std::string where = line.substr(0, kind);   // file:line:col
        size_t c2 = where.rfind(':');
        size_t c1 = c2 == std::string::npos || c2 == 0 ? std::string::npos : where.rfind(':', c2 - 1);
        std::string file = where;
        if (c1 != std::string::npos) {
            file = where.substr(0, c1);
            cp.line = std::atoi(where.substr(c1 + 1, c2 - c1 - 1).c_str());
            cp.col = std::atoi(where.substr(c2 + 1).c_str());
        } else if (c2 != std::string::npos) {
            file = where.substr(0, c2);
            cp.line = std::atoi(where.substr(c2 + 1).c_str());
        }
        size_t slash = file.find_last_of("/\\");
        if (slash != std::string::npos) file = file.substr(slash + 1);
        if (file == "tests") cp.inTests = true;
        else if (file != "solution.cpp") cp.line = 0;   // somewhere else (a header): no line of theirs to point at
        if (!cp.error && file != "solution.cpp") continue;   // warnings in our own code aren't theirs to fix
        out.push_back(cp);
        if (out.size() >= 50) break;
    }
    return out;
}

std::string crash_explanation(const std::string &crash)
{
    if (crash == "segmentation fault" || crash == "bus error" || crash == "array out of bounds")
        return "Your code used memory it doesn't own: following a pointer to nothing (nullptr), going past the end "
               "of a list or array, or using memory after it was deleted.";
    if (crash == "stack overflow")
        return "A function kept calling itself and never stopped (recursion with no base case), or a huge array was "
               "made inside a function.";
    if (crash == "divide by zero") return "Something was divided by zero, or % 0 was used.";
    if (crash == "uncaught exception") return "Something threw an exception that nothing caught (for example .at() past the end).";
    if (crash == "abort") return "The program stopped itself: a failed assert, or a delete of memory that wasn't new'd.";
    if (crash == "timeout") return "This test never finished: usually a loop whose condition never becomes false, "
                                   "or a pointer that never moves on to the next node.";
    return "Your code stopped part way through this test.";
}

// A crash with no @@CRASH line (the program was killed): name it from the exit code
static std::string crash_from_exit(int status)
{
#ifdef _WIN32
    unsigned int code = (unsigned int)status;
    if (code == 0xC0000005u) return "segmentation fault";
    if (code == 0xC00000FDu) return "stack overflow";
    if (code == 0xC0000094u) return "divide by zero";
    if (code == 3) return "abort";
#else
    if (status == 128 + 11) return "segmentation fault";
    if (status == 128 + 8) return "divide by zero";
    if (status == 128 + 6) return "abort";
    if (status == 128 + 7) return "bus error";
#endif
    return "crash";
}

// ---------------------------------------------------------------------------
// Running an answer
// ---------------------------------------------------------------------------

static void live_log(RunProgress *live, const std::string &line, std::vector<std::string> &log)
{
    log.push_back(line);
    if (!live) return;
    std::lock_guard<std::mutex> g(live->lock);
    live->log.push_back(line);
}

static void live_phase(RunProgress *live, const std::string &phase)
{
    if (!live) return;
    std::lock_guard<std::mutex> g(live->lock);
    live->phase = phase;
}

static void live_state(RunProgress *live, int test, int state)
{
    if (!live || test < 0) return;
    std::lock_guard<std::mutex> g(live->lock);
    if (test >= (int)live->state.size()) live->state.resize(test + 1, 0);
    live->state[test] = state;
    if (test >= live->total) live->total = test + 1;
}

static std::string seconds_text(double s)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%.1f s", s);
    return buf;
}

RunResult run_answer(const Problem &p, const std::string &code, const std::string &workDir, RunProgress *live)
{
    RunResult r;
    auto start = std::chrono::steady_clock::now();
    auto elapsed = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); };
    r.ruleProblems = check_rules(p, code);
    std::vector<CheckInfo> checks = split_checks(p);
    if (live) {
        std::lock_guard<std::mutex> g(live->lock);
        live->total = (int)checks.size();
        live->state.assign(checks.size(), 0);
    }

    const std::string &cxx = find_compiler();
    if (cxx.empty()) {
        r.status = RunStatus::NoCompiler;
        r.compilerOutput = "No C++ compiler found. Stack Trace looks for c++, g++ or clang++, and on Windows "
                           "for C:/raylib/w64devkit/bin/g++.exe. Set STACKTRACE_CXX to your compiler to choose one.";
        live_log(live, r.compilerOutput, r.log);
        if (live) { std::lock_guard<std::mutex> g(live->lock); live->done = true; }
        return r;
    }

    std::error_code ec;
    std::filesystem::create_directories(workDir, ec);
    std::string source = workDir + "/solution_test.cpp";
#ifdef _WIN32
    std::string exe = workDir + "/solution.exe";
    std::string runCmd = exe;
    for (char &c : runCmd) if (c == '/') c = '\\';
    std::string extra = " -static";   // so it runs without the compiler's DLLs on the PATH
    std::string noInput = " < nul";
#else
    std::string exe = workDir + "/solution";
    std::string runCmd = (exe[0] == '/') ? exe : "./" + exe;
    std::string extra = "";
    std::string noInput = " < /dev/null";
#endif

    // Compile, unless it's exactly what was compiled last time (Run, then Submit)
    std::string program = build_test_program(p, code, TEST_SECONDS);
    std::string old;
    bool same = read_file(source, old) && old == program && std::filesystem::exists(exe, ec);
    if (same) {
        live_log(live, "$ g++ solution.cpp     (no changes since the last run: using that build)", r.log);
    } else {
        std::remove(exe.c_str());   // never run an old build if this one fails
        { std::ofstream f(source); f << program; }
        live_phase(live, "Compiling");
        live_log(live, "$ g++ -std=c++17 solution.cpp", r.log);
        std::string compileOut;
        int status = run_command(cxx + " -std=c++17 -O0" + extra + " -o " + exe + " " + source, compileOut);
        r.compilerOutput = compileOut;
        r.problems = parse_compile_problems(compileOut);
        if (status != 0) {
            std::remove(source.c_str());   // so the same broken code is compiled (and explained) again next time
            r.status = RunStatus::CompileError;
            int errors = 0;
            for (const CompileProblem &cp : r.problems) if (cp.error) errors++;
            for (const CompileProblem &cp : r.problems) {
                if (!cp.error) continue;
                live_log(live, std::string("  error  ") + (cp.inTests ? "in the tests" : cp.line > 0 ? "line " + std::to_string(cp.line) : "") +
                         ": " + cp.message, r.log);
            }
            live_log(live, "Didn't compile: " + std::to_string(errors) + (errors == 1 ? " error" : " errors") + " (" +
                     seconds_text(elapsed()) + ")", r.log);
            r.seconds = elapsed();
            if (live) { std::lock_guard<std::mutex> g(live->lock); live->done = true; }
            return r;
        }
        live_log(live, "  compiled (" + seconds_text(elapsed()) + ")", r.log);
    }

    // Run it. Each run goes until it finishes, or crashes / hangs in a test;
    // then that test is marked and the next run starts from the test after.
    std::vector<TestResult> tests;
    std::string output;
    auto test_at = [&](int k) -> TestResult & {
        if (k >= (int)tests.size()) tests.resize(k + 1);
        return tests[k];
    };
    auto describe = [&](int k) {
        return k < (int)checks.size() && !checks[k].description.empty() ? checks[k].description : "test " + std::to_string(k + 1);
    };
    int from = 0, total = (int)checks.size();
    bool finished = false;
    std::vector<bool> got;   // tests with a result
    for (int attempt = 0; attempt < 60 && !finished; attempt++) {
        int at = -1;
        bool timeout = false, summary = false, gotAt = false;
        std::string crash;
        live_log(live, "$ ./solution" + (from > 0 ? " " + std::to_string(from) + "     (carrying on from test " + std::to_string(from + 1) + ")" : std::string()), r.log);
        auto onLine = [&](const std::string &line) {
            if (line.compare(0, 5, "@@AT\t") == 0) {
                at = std::atoi(line.c_str() + 5);
                gotAt = true;
                if (at >= from) {
                    live_phase(live, "Running test " + std::to_string(at + 1) + (total > 0 ? " of " + std::to_string(std::max(total, at + 1)) : ""));
                }
            } else if (line.compare(0, 7, "@@PASS\t") == 0 || line.compare(0, 7, "@@FAIL\t") == 0) {
                RunResult one;
                parse_test_output(line + "\n", one);
                if (one.tests.empty() || at < 0) return;
                TestResult t = one.tests[0];
                t.outcome = t.passed ? TestOutcome::Passed : TestOutcome::Failed;
                test_at(at) = t;
                if ((int)got.size() <= at) got.resize(at + 1, false);
                got[at] = true;
                live_state(live, at, t.passed ? 1 : 2);
                live_log(live, std::string(t.passed ? "  PASS  " : "  FAIL  ") + std::to_string(at + 1) + "  " + t.description, r.log);
            } else if (line.compare(0, 8, "@@CRASH\t") == 0) {
                crash = line.substr(8);
            } else if (line == "@@TIMEOUT") {
                timeout = true;
            } else if (line.compare(0, 10, "@@SUMMARY\t") == 0) {
                summary = true;
                std::vector<std::string> f;
                std::stringstream ss(line.substr(10));
                std::string field;
                while (std::getline(ss, field, '\t')) f.push_back(field);
                if (f.size() >= 3) total = std::atoi(f[2].c_str());
            } else if (!line.empty()) {
                if (output.size() < 20000) output += line + "\n";
                live_log(live, "  | " + line, r.log);
            }
        };
        std::string raw;
        int status = run_command(runCmd + " " + std::to_string(from) + noInput, raw, onLine);
        if (summary) { finished = true; break; }

        // It stopped part way: which test was it in?
        if (crash.empty() && !timeout) crash = crash_from_exit(status);
        bool inCheck = gotAt && at >= from && !(at < (int)got.size() && got[at]);
        int culprit = inCheck ? at : at + 1;   // in a check, or in the setup lines after the last one
        if (culprit < from) {
            // It died before reaching the test to carry on from, so it'd do the same every time
            for (int k = from; k < std::max(total, from + 1); k++) {
                TestResult &t = test_at(k);
                t = TestResult();
                t.outcome = TestOutcome::NotRun;
                t.description = describe(k);
                t.code = k < (int)checks.size() ? checks[k].code : "";
                live_state(live, k, 5);
            }
            live_log(live, "  The code before test " + std::to_string(from + 1) + " " + (timeout ? "hangs" : "crashes") +
                     " every time, so the rest couldn't run.", r.log);
            break;
        }
        TestResult &t = test_at(culprit);
        t = TestResult();
        t.outcome = timeout ? TestOutcome::TimedOut : TestOutcome::Crashed;
        t.crash = timeout ? "timeout" : crash;
        t.description = describe(culprit);
        t.code = culprit < (int)checks.size() ? checks[culprit].code : "";
        if ((int)got.size() <= culprit) got.resize(culprit + 1, false);
        got[culprit] = true;
        if (r.crashedAt < 0) r.crashedAt = culprit;
        live_state(live, culprit, timeout ? 4 : 3);
        live_log(live, std::string(timeout ? "  TIME  " : "  CRASH ") + std::to_string(culprit + 1) + "  " + t.description + "  (" +
                 (timeout ? "never finished: stopped after " + std::to_string(TEST_SECONDS) + " s" : t.crash) + ")", r.log);
        if (timeout) r.status = RunStatus::TimedOut;
        from = culprit + 1;
        if (total > 0 && from >= total) break;
        if (elapsed() > RUN_BUDGET_SECONDS) {
            for (int k = from; k < total; k++) {
                TestResult &n = test_at(k);
                n = TestResult();
                n.outcome = TestOutcome::NotRun;
                n.description = describe(k);
                live_state(live, k, 5);
            }
            live_log(live, "  Out of time: the rest weren't run.", r.log);
            break;
        }
    }

    // Every test, in order; any without a result (shouldn't happen) are "not run"
    if ((int)tests.size() < total) tests.resize(total);
    for (int k = 0; k < (int)tests.size(); k++) {
        if (k < (int)got.size() && got[k]) continue;
        if (tests[k].outcome == TestOutcome::NotRun && !tests[k].description.empty()) continue;
        tests[k] = TestResult();
        tests[k].outcome = TestOutcome::NotRun;
        tests[k].description = describe(k);
    }
    r.tests = tests;
    r.programOutput = output;
    r.passed = r.failed = 0;
    bool anyCrash = false, anyTimeout = false;
    for (const TestResult &t : r.tests) {
        if (t.passed) r.passed++;
        else r.failed++;
        if (t.outcome == TestOutcome::Crashed || t.outcome == TestOutcome::NotRun) anyCrash = true;
        if (t.outcome == TestOutcome::TimedOut) anyTimeout = true;
    }
    if (anyTimeout && !anyCrash) r.status = RunStatus::TimedOut;
    else if (anyCrash) r.status = RunStatus::Crashed;
    else if (anyTimeout) r.status = RunStatus::TimedOut;
    else r.status = r.failed == 0 ? RunStatus::Passed : RunStatus::Failed;

    r.seconds = elapsed();
    live_log(live, "Done: " + std::to_string(r.passed) + " of " + std::to_string((int)r.tests.size()) + " passed (" +
             seconds_text(r.seconds) + ")", r.log);
    if (live) { std::lock_guard<std::mutex> g(live->lock); live->done = true; live->phase = "Done"; }
    return r;
}

// ---------------------------------------------------------------------------
// MAL-OS's gcc: a plain C program, compiled and run
// ---------------------------------------------------------------------------

// Compiled in alongside the student's program: stops it after 3 seconds.
static const char *WATCHDOG_C =
    "#include <stdio.h>\n"
    "#ifdef _WIN32\n"
    "#include <windows.h>\n"
    "static DWORD WINAPI st_watch(LPVOID p) { (void)p; Sleep(3000);\n"
    "    printf(\"\\n[MAL-OS stopped it after 3 seconds: an endless loop?]\\n\"); fflush(stdout); ExitProcess(124); return 0; }\n"
    "__attribute__((constructor)) static void st_start(void) { CreateThread(0, 0, st_watch, 0, 0, 0); }\n"
    "#else\n"
    "#include <signal.h>\n"
    "#include <unistd.h>\n"
    "static void st_alarm(int s) { (void)s;\n"
    "    printf(\"\\n[MAL-OS stopped it after 3 seconds: an endless loop?]\\n\"); fflush(stdout); _exit(124); }\n"
    "__attribute__((constructor)) static void st_start(void) { signal(SIGALRM, st_alarm); alarm(3); }\n"
    "#endif\n";

static std::string c_exe_path(const std::string &workDir)
{
#ifdef _WIN32
    return workDir + "/a.exe";
#else
    return workDir + "/a.out";
#endif
}

bool compile_c_program(const std::string &code, const std::string &workDir, std::string &output, bool cpp)
{
    const std::string &cxx = find_compiler();
    if (cxx.empty()) {
        output = "gcc: no compiler found on this computer (MAL-OS uses the same one as Exam prep).";
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(workDir, ec);
    const std::string file = cpp ? "hello.cpp" : "hello.c";
    { std::ofstream f(workDir + "/" + file); f << code; }
    { std::ofstream f(workDir + "/watchdog.c"); f << WATCHDOG_C; }
    std::string exe = c_exe_path(workDir);
    std::remove(exe.c_str());
#ifdef _WIN32
    std::string extra = " -static";
#else
    std::string extra = "";
#endif
    // -x c: compile as C even though the compiler found is a C++ one
    int status = run_command(cxx + (cpp ? " -x c++ -std=c++17" : " -x c") + " -Wall -O0" + extra + " -o " + exe + " " + workDir + "/" +
                             file + " -x c " + workDir + "/watchdog.c", output);
    // Show the student's file name, not the work folder
    std::string from = workDir + "/" + file;
    for (size_t at = output.find(from); at != std::string::npos; at = output.find(from, at))
        output.replace(at, from.size(), file);
    return status == 0;
}

std::string run_c_program(const std::string &workDir)
{
    std::string exe = c_exe_path(workDir), out;
#ifdef _WIN32
    std::string cmd = exe;
    for (char &c : cmd) if (c == '/') c = '\\';
    run_command(cmd + " < nul", out);
#else
    std::string cmd = (exe[0] == '/') ? exe : "./" + exe;
    run_command(cmd + " < /dev/null", out);
#endif
    return out;
}
