// content.h - topics, lessons and quiz questions, loaded from the files in content/.
//
// No drawing in here (no raylib), so the tests can load content without a window.
// The file formats are described in README.md under "Writing content".

#pragma once
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Topics (content/topics.txt)
// ---------------------------------------------------------------------------

struct Topic {
    std::string id;          // short name used everywhere else, e.g. "pointers"
    std::string title;       // shown on screen, e.g. "Pointers & addresses"
    std::string track;       // which row of the topic map, e.g. "Pointers & memory"
    std::string exam;        // "Exam 1", "Exam 2" or "" if not tied to an exam
    std::string lessonFile;  // path inside content/, or "" if none yet
    std::string quizFile;    // path inside content/, or "" if none yet
};

// ---------------------------------------------------------------------------
// Lessons (content/lessons/*.txt)
// ---------------------------------------------------------------------------

// One box in the memory view: a variable on the stack, or a block on the heap.
struct MemCell {
    std::string area;     // the stack frame ("main", "swap") or "heap"
    std::string name;     // "i", "ptr", "node1"
    std::string type;     // "int", "int*"
    std::string address;  // "1000"
    std::string value;    // "5", or "->i" when it points at the cell called i
    bool changed = false; // highlight it: it changed on this step
};

// The state of memory after one line of the lesson's code has run.
struct Step {
    int line = 0;               // which line of the code is highlighted (1 = first, 0 = none)
    std::string explain;        // what just happened, in plain words
    std::vector<MemCell> cells; // everything in memory at this point
};

struct Lesson {
    std::string title;
    std::string topic;              // the topic id it belongs to
    std::string text;               // the short tutorial text shown before the steps
    std::vector<std::string> code;  // the code, one string per line
    std::vector<Step> steps;
};

// ---------------------------------------------------------------------------
// Quizzes (content/quizzes/*.txt)
// ---------------------------------------------------------------------------

enum class QuestionKind {
    Choice,  // pick one of the options
    Line     // click the line of code that's the answer (spot the bug)
};

struct Question {
    QuestionKind kind = QuestionKind::Choice;
    std::string mode;                  // shown as a tag: "Predict the output", "Spot the bug"...
    std::string prompt;
    std::vector<std::string> code;     // may be empty
    std::vector<std::string> options;  // Choice questions only
    int answer = 0;                    // Choice: option number (1 = first). Line: line number (1 = first)
    std::string explain;               // shown after answering
};

struct Quiz {
    std::string title;
    std::string topic;
    std::vector<Question> questions;
};

// ---------------------------------------------------------------------------
// Loading. Each returns false (and leaves a message in error) if the file is
// missing or nothing usable was found in it. Bad lines are skipped, not fatal.
// ---------------------------------------------------------------------------

bool load_topics(const std::string &path, std::vector<Topic> &topics, std::string &error);
bool load_lesson(const std::string &path, Lesson &lesson, std::string &error);
bool load_quiz(const std::string &path, Quiz &quiz, std::string &error);

// The same, from text already in memory (used by the tests).
bool parse_topics(const std::string &text, std::vector<Topic> &topics, std::string &error);
bool parse_lesson(const std::string &text, Lesson &lesson, std::string &error);
bool parse_quiz(const std::string &text, Quiz &quiz, std::string &error);

// Helpers the screens use too.
std::vector<std::string> split_fields(const std::string &line, char sep);  // split and trim
std::string trim(const std::string &s);
