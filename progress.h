// progress.h - what the student has done: mastery points per topic and a log
// of every answer with its date (for the profile's activity heatmap).
// Saved to progress.txt next to the program. No drawing in here.

#pragma once
#include <map>
#include <string>
#include <vector>

const int POINTS_CORRECT = 10;    // gained for a right answer
const int POINTS_WRONG = 5;       // lost for a wrong one (never below 0)
const int POINTS_MAX = 100;       // per topic

// Khan Academy style levels, from the points on a topic.
enum class Mastery { NotStarted, Attempted, Familiar, Proficient, Mastered };

struct AnswerRecord {
    std::string date;    // "2026-11-03"
    std::string topic;   // topic id
    bool correct = false;
};

struct Progress {
    std::map<std::string, int> points;  // topic id -> 0..100
    std::vector<AnswerRecord> answers;
};

// A missing file just means a new student: returns true with empty progress.
// Returns false only if the file exists but can't be read.
bool progress_load(const std::string &path, Progress &p, std::string &error);
bool progress_save(const std::string &path, const Progress &p, std::string &error);

// Record one answer: adds it to the log and changes the topic's points.
void progress_record(Progress &p, const std::string &topic, bool correct, const std::string &date);

int topic_points(const Progress &p, const std::string &topic);
Mastery topic_mastery(const Progress &p, const std::string &topic);
const char *mastery_name(Mastery m);   // "Not started", "Familiar"...

// Today's date as "YYYY-MM-DD".
std::string today_string();
