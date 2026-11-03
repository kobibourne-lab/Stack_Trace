// content.cpp - reads topics.txt, lesson files and quiz files.
//
// Every file is plain text, one "KEY: value" per line. Lines starting with #
// are comments. Code goes between a CODE: line and an END line, and is kept
// exactly as written (apart from tabs, which become four spaces).

#include "content.h"
#include <fstream>
#include <sstream>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Small text helpers
// ---------------------------------------------------------------------------

std::string trim(const std::string &s)
{
    size_t start = 0, end = s.size();
    while (start < end && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) start++;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) end--;
    return s.substr(start, end - start);
}

std::vector<std::string> split_fields(const std::string &line, char sep)
{
    std::vector<std::string> fields;
    std::string field;
    std::stringstream ss(line);
    while (std::getline(ss, field, sep)) fields.push_back(trim(field));
    return fields;
}

// Split text into lines, removing Windows line endings (\r).
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

// Code lines keep their indentation; tabs become 4 spaces so they line up.
static std::string untab(const std::string &line)
{
    std::string out;
    for (char c : line) {
        if (c == '\t') out += "    ";
        else out += c;
    }
    return out;
}

// "KEY: value" -> key "KEY", value "value". Returns false if there's no colon.
static bool split_key(const std::string &line, std::string &key, std::string &value)
{
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;
    key = trim(line.substr(0, colon));
    value = trim(line.substr(colon + 1));
    return true;
}

// Read a whole number, or return fallback if it isn't one.
static int to_int(const std::string &s, int fallback)
{
    char *end = nullptr;
    long n = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') return fallback;
    return (int)n;
}

static bool read_file(const std::string &path, std::string &text, std::string &error)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "could not open " + path;
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    text = ss.str();
    return true;
}

// Appends a paragraph, with a blank line between paragraphs.
static void add_text(std::string &to, const std::string &more)
{
    if (!to.empty()) to += "\n";
    to += more;
}

// ---------------------------------------------------------------------------
// topics.txt:   id | title | track | exam | lesson file | quiz file
// ---------------------------------------------------------------------------

bool parse_topics(const std::string &text, std::vector<Topic> &topics, std::string &error)
{
    topics.clear();
    for (const std::string &raw : split_lines(text)) {
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> f = split_fields(line, '|');
        if (f.size() < 3 || f[0].empty()) continue;   // need at least id, title, track

        Topic t;
        t.id = f[0];
        t.title = f[1];
        t.track = f[2];
        if (f.size() > 3 && f[3] != "-") t.exam = f[3];
        if (f.size() > 4 && f[4] != "-") t.lessonFile = f[4];
        if (f.size() > 5 && f[5] != "-") t.quizFile = f[5];
        topics.push_back(t);
    }
    if (topics.empty()) {
        error = "no topics found";
        return false;
    }
    return true;
}

bool load_topics(const std::string &path, std::vector<Topic> &topics, std::string &error)
{
    std::string text;
    if (!read_file(path, text, error)) return false;
    return parse_topics(text, topics, error);
}

// ---------------------------------------------------------------------------
// Lessons
// ---------------------------------------------------------------------------

bool parse_lesson(const std::string &text, Lesson &lesson, std::string &error)
{
    lesson = Lesson();
    std::vector<std::string> lines = split_lines(text);
    bool inCode = false;

    for (const std::string &raw : lines) {
        if (inCode) {
            if (trim(raw) == "END") inCode = false;
            else lesson.code.push_back(untab(raw));
            continue;
        }

        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;

        std::string key, value;
        if (!split_key(line, key, value)) continue;

        if (key == "TITLE") lesson.title = value;
        else if (key == "TOPIC") lesson.topic = value;
        else if (key == "TEXT") add_text(lesson.text, value);
        else if (key == "CODE") inCode = true;
        else if (key == "STEP") {
            Step s;
            s.line = to_int(value, 0);
            lesson.steps.push_back(s);
        }
        else if (key == "EXPLAIN" && !lesson.steps.empty()) add_text(lesson.steps.back().explain, value);
        else if (key == "MEM" && !lesson.steps.empty()) {
            // MEM: area | name | type | address | value [| changed]
            std::vector<std::string> f = split_fields(value, '|');
            if (f.size() < 5) continue;
            MemCell c;
            c.area = f[0];
            c.name = f[1];
            c.type = f[2];
            c.address = f[3];
            c.value = f[4];
            c.changed = f.size() > 5 && f[5] == "changed";
            lesson.steps.back().cells.push_back(c);
        }
    }

    // Steps that point past the end of the code don't highlight anything.
    for (Step &s : lesson.steps)
        if (s.line < 0 || s.line > (int)lesson.code.size()) s.line = 0;

    if (lesson.title.empty()) { error = "lesson has no TITLE"; return false; }
    if (lesson.steps.empty()) { error = "lesson has no STEP"; return false; }
    return true;
}

bool load_lesson(const std::string &path, Lesson &lesson, std::string &error)
{
    std::string text;
    if (!read_file(path, text, error)) return false;
    if (!parse_lesson(text, lesson, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Quizzes
// ---------------------------------------------------------------------------

// A question is only kept if it can actually be answered.
static bool question_ok(const Question &q)
{
    if (q.prompt.empty()) return false;
    if (q.kind == QuestionKind::Choice)
        return q.options.size() >= 2 && q.answer >= 1 && q.answer <= (int)q.options.size();
    return !q.code.empty() && q.answer >= 1 && q.answer <= (int)q.code.size();
}

bool parse_quiz(const std::string &text, Quiz &quiz, std::string &error)
{
    quiz = Quiz();
    std::vector<std::string> lines = split_lines(text);
    std::vector<Question> found;
    bool inCode = false;
    int skipped = 0;

    for (const std::string &raw : lines) {
        if (inCode) {
            if (trim(raw) == "END") inCode = false;
            else if (!found.empty()) found.back().code.push_back(untab(raw));
            continue;
        }

        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;

        std::string key, value;
        if (!split_key(line, key, value)) continue;

        if (key == "TITLE") { quiz.title = value; continue; }
        if (key == "TOPIC") { quiz.topic = value; continue; }
        if (key == "QUESTION") {
            Question q;
            q.mode = value;
            found.push_back(q);
            continue;
        }
        if (found.empty()) continue;   // anything else needs a QUESTION first
        Question &q = found.back();

        if (key == "KIND") q.kind = (value == "line") ? QuestionKind::Line : QuestionKind::Choice;
        else if (key == "PROMPT") add_text(q.prompt, value);
        else if (key == "CODE") inCode = true;
        else if (key == "OPTION") q.options.push_back(value);
        else if (key == "ANSWER") q.answer = to_int(value, 0);
        else if (key == "EXPLAIN") add_text(q.explain, value);
    }

    for (const Question &q : found) {
        if (question_ok(q)) quiz.questions.push_back(q);
        else skipped++;
    }

    if (quiz.questions.empty()) {
        error = "no usable questions";
        if (skipped > 0) error += " (" + std::to_string(skipped) + " skipped: check OPTION and ANSWER)";
        return false;
    }
    return true;
}

bool load_quiz(const std::string &path, Quiz &quiz, std::string &error)
{
    std::string text;
    if (!read_file(path, text, error)) return false;
    if (!parse_quiz(text, quiz, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}
