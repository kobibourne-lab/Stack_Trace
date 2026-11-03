// practice.cpp - loading problems and testing answers. See practice.h.
//
// How an answer is tested:
//   1. build_test_program() writes one .cpp file: some test helpers, the
//      student's code (with #line so compiler errors show their own line
//      numbers), then the problem's tests.
//   2. run_answer() compiles it with the computer's C++ compiler and runs it.
//   3. The test program prints one line per test, starting @@PASS or @@FAIL,
//      and parse_test_output() turns those back into results.
// A watchdog in the test program stops it after a few seconds, so an endless
// loop shows "Time limit exceeded" instead of freezing the app.

#include "practice.h"
#include "content.h"   // trim, split_fields
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
    #define ST_POPEN _popen
    #define ST_PCLOSE _pclose
#else
    #include <sys/wait.h>
    #define ST_POPEN popen
    #define ST_PCLOSE pclose
#endif

const int TIME_LIMIT_SECONDS = 5;

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
            // SET: id | title | exam
            std::vector<std::string> f = split_fields(line.substr(4), '|');
            ProblemSet s;
            s.id = f.size() > 0 ? f[0] : "";
            s.title = f.size() > 1 ? f[1] : s.id;
            if (f.size() > 2 && f[2] != "-") s.exam = f[2];
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

    // Sets with nothing in them aren't worth showing
    std::vector<ProblemSet> kept;
    for (const ProblemSet &s : sets) if (!s.problems.empty()) kept.push_back(s);
    sets = kept;
    if (sets.empty() && error.empty()) error = "no problems found";
    return !sets.empty();
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

// The end: a watchdog so endless loops stop, then main().
static std::string footer(int seconds)
{
    std::string s = R"ST(
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
static DWORD WINAPI st_watchdog(LPVOID)
{
    Sleep(ST_LIMIT * 1000);
    printf("\n@@TIMEOUT\n");
    fflush(stdout);
    ExitProcess(124);
    return 0;
}
static void st_start_watchdog() { CreateThread(NULL, 0, st_watchdog, NULL, 0, NULL); }
#else
#include <signal.h>
#include <unistd.h>
static void st_on_alarm(int)
{
    const char m[] = "\n@@TIMEOUT\n";
    if (write(1, m, sizeof m - 1) < 0) {}
    _exit(124);
}
static void st_start_watchdog() { signal(SIGALRM, st_on_alarm); alarm(ST_LIMIT); }
#endif

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);   // so nothing is lost if the code crashes
    st_start_watchdog();
    st_tests();
    printf("\n@@SUMMARY\t%d\t%d\n", st_passed, st_failed);
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
static int run_command(const std::string &command, std::string &output)
{
    output.clear();
    std::string full = command + " 2>&1";
    FILE *pipe = ST_POPEN(full.c_str(), "r");
    if (pipe == nullptr) return -1;
    char buf[1024];
    while (fgets(buf, sizeof buf, pipe) != nullptr) {
        if (output.size() < 200000) output += buf;
    }
    int status = ST_PCLOSE(pipe);
#ifndef _WIN32
    if (status != -1) status = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
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

RunResult run_answer(const Problem &p, const std::string &code, const std::string &workDir)
{
    RunResult r;
    auto start = std::chrono::steady_clock::now();
    r.ruleProblems = check_rules(p, code);

    const std::string &cxx = find_compiler();
    if (cxx.empty()) {
        r.status = RunStatus::NoCompiler;
        r.compilerOutput = "No C++ compiler found. Stack Trace looks for c++, g++ or clang++, and on Windows "
                           "for C:/raylib/w64devkit/bin/g++.exe. Set STACKTRACE_CXX to your compiler to choose one.";
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
#else
    std::string exe = workDir + "/solution";
    std::string runCmd = (exe[0] == '/') ? exe : "./" + exe;
    std::string extra = "";
#endif
    std::remove(exe.c_str());   // never run an old build if this one fails

    {
        std::ofstream f(source);
        f << build_test_program(p, code, TIME_LIMIT_SECONDS);
    }

    std::string compileOut;
    int status = run_command(cxx + " -std=c++17 -O0" + extra + " -o " + exe + " " + source, compileOut);
    r.compilerOutput = compileOut;
    if (status != 0) {
        r.status = RunStatus::CompileError;
    } else {
        std::string runOut;
        run_command(runCmd, runOut);
        parse_test_output(runOut, r);
    }

    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return r;
}
