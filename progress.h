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

// An Exam prep problem that has been solved.
struct SolvedRecord {
    std::string date;    // when it was first solved
    int seconds = 0;     // the fastest time it was solved in
};

// A campaign boss that has been beaten.
struct BossRecord {
    std::string date;    // when it was first beaten
    int seconds = 0;     // the fastest win
    int stars = 0;       // the most stars won (1 to 3)
};

struct Progress {
    std::map<std::string, int> points;  // topic id -> 0..100
    std::map<std::string, int> scores;  // MAL-OS minigame -> best score
    std::string hat = "none", skin = "classic", pet = "nopet";   // what Mal is wearing (the wardrobe)
    std::vector<AnswerRecord> answers;
    std::map<std::string, SolvedRecord> solved;   // problem id ("lab1/q01_leap_year") -> record
    std::map<std::string, BossRecord> bosses;     // campaign region id ("stack") -> record
};

// A missing file just means a new student: returns true with empty progress.
// Returns false only if the file exists but can't be read.
bool progress_load(const std::string &path, Progress &p, std::string &error);
bool progress_save(const std::string &path, const Progress &p, std::string &error);

// Record one answer: adds it to the log and changes the topic's points.
void progress_record(Progress &p, const std::string &topic, bool correct, const std::string &date);

// Marks an Exam prep problem solved, keeping the fastest time.
// Returns true if it's the first time it has been solved.
bool progress_mark_solved(Progress &p, const std::string &problemId, const std::string &date, int seconds);
bool progress_is_solved(const Progress &p, const std::string &problemId);

// Records a campaign boss beaten, keeping the fastest time and the most stars.
// Returns true if it's the first time it has been beaten.
bool progress_record_boss(Progress &p, const std::string &regionId, const std::string &date, int seconds, int stars);
bool progress_boss_beaten(const Progress &p, const std::string &regionId);

// A minigame score: keeps the best. Returns true if it's a new best.
bool progress_record_score(Progress &p, const std::string &game, int score);
int progress_best_score(const Progress &p, const std::string &game);
int progress_boss_stars(const Progress &p, const std::string &regionId);   // 0 if not beaten

int topic_points(const Progress &p, const std::string &topic);
Mastery topic_mastery(const Progress &p, const std::string &topic);
const char *mastery_name(Mastery m);   // "Not started", "Familiar"...

// Today's date as "YYYY-MM-DD".
std::string today_string();

// ---------------------------------------------------------------------------
// Dates and activity, for the Profile screen. Dates are "YYYY-MM-DD" strings.
// ---------------------------------------------------------------------------

// Days since 1970-01-01, or -1 if the text isn't a valid date.
int date_to_days(const std::string &date);
std::string days_to_date(int days);
std::string date_add_days(const std::string &date, int n);   // n can be negative

// 0 = Monday ... 6 = Sunday (the heatmap's rows).
int date_weekday(const std::string &date);

// How many answers were given on each date.
std::map<std::string, int> answers_per_day(const Progress &p);

// Days in a row with at least one answer, counting back from today. If there's
// nothing today yet, the streak is still alive from yesterday (like GitHub).
int progress_streak(const Progress &p, const std::string &today);

int progress_total_points(const Progress &p);

// ---------------------------------------------------------------------------
// Finishing a quiz: mastery from the lives left.
// ---------------------------------------------------------------------------

// Passing a quiz (at least one life left) sets its topic's mastery from the
// lives left: all of them is 100 (Mastered), and each life lost takes 20 off
// (2 of 3 left is 80, 1 left is 60). A pass is always 60 or more, so it always
// clears the topic in the campaign (which clears at 50). It only ever raises
// the points, never lowers them. Returns false if no lives were left.
int quiz_mastery_points(int livesLeft, int maxLives);
bool progress_finish_quiz(Progress &p, const std::string &topic, int livesLeft, int maxLives);

// ---------------------------------------------------------------------------
// XP and levels, RPG style. XP only ever goes up (unlike a topic's points,
// which drop for wrong answers), and it's worked out from what's already in
// progress.txt, so the file format doesn't change.
// ---------------------------------------------------------------------------

const int XP_CORRECT = 10;   // a right quiz answer
const int XP_WRONG = 2;      // a wrong one still counts for having a go
const int XP_SOLVED = 25;    // an Exam prep problem solved
const int XP_BOSS = 150;     // a campaign boss beaten
const int XP_STAR = 25;      // each star won against a boss (the best run counts)

int progress_xp(const Progress &p);

// Level 1 starts at 0 XP. Each level needs 50 XP more than the one before:
// level 2 at 50, level 3 at 150, level 4 at 300, level 5 at 500 ...
int xp_for_level(int level);
int level_for_xp(int xp);

// A title for each level, e.g. "Pointer Apprentice" at level 5.
std::string level_title(int level);
int progress_correct_count(const Progress &p);
