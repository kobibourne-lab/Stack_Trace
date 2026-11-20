// examq.h - the Exam question of the day: one question a day, picked from ALL
// the material in the app (every quiz question of every topic, not just the
// labs), the same for everyone on that date, like Codle. It's an exam, so you
// get ONE go: no lives, no second try, and the topic is only shown once you've
// answered. A right answer pays bytes (more for a streak of days in a row);
// a wrong one still pays a few for having a go, and shows the explanation.
//
// The question changes at midnight (every 24 hours). The day's pick comes from
// shuffling the whole pool once per cycle, so nothing comes round twice until
// every question has been used.
//
// No drawing here (screen_examq.cpp draws it), so it can be tested.

#pragma once
#include <string>
#include <vector>
#include "content.h"
#include "progress.h"

const std::string EXAMQ_START = "2026-11-20";   // question #1
const int EXAMQ_BYTES = 20;          // a right answer
const int EXAMQ_STREAK_BONUS = 5;    // extra for each day already in the streak...
const int EXAMQ_STREAK_MAX = 5;      // ...up to this many days of it
const int EXAMQ_TRY_BYTES = 5;       // a wrong answer still pays this

// One question in the pool, and where it came from.
struct ExamQ {
    std::string topic;        // topic id ("pointers")
    std::string topicTitle;   // "Pointers & addresses"
    std::string exam;         // "Exam 1", "Exam 2", or "" (the newer topics aren't tied to an exam)
    int number = 0;           // its place in that topic's quiz (1 = first)
    Question q;
};

// Load every question of every topic's quiz (contentDir is "content"). The
// order is topics.txt's, then each quiz's. Returns false (with why) if there
// are no questions at all. A quiz that won't load is skipped.
bool examq_load(const std::vector<Topic> &topics, const std::string &contentDir, std::vector<ExamQ> &pool, std::string &error);

// Which question (an index into the pool) that date has. The same for
// everyone, and no repeats until the whole pool has been used.
int examq_index(int poolSize, const std::string &date);

// Its number: #1 on EXAMQ_START.
int examq_number(const std::string &date);

// Has today's question been answered, was it right, and what was picked
// (the option, or the line for a spot-the-bug one)?
bool examq_answered(const Progress &p, const std::string &date);
bool examq_correct(const Progress &p, const std::string &date);
int examq_choice(const Progress &p, const std::string &date);

// Days in a row answered correctly. Still alive today if yesterday was right
// and today isn't answered yet; a wrong answer or a missed day ends it.
int examq_streak(const Progress &p, const std::string &today);

// The bytes an answer pays. streakAfter is the streak once this answer counts.
int examq_reward(bool correct, int streakAfter);

// Answer today's question. Returns false with why if it can't be (already
// answered, or the choice isn't one of the options / lines). Otherwise it
// marks it, updates the stats and streak, pays bytes (bytesWon says how
// many), and fills correct. The topic's mastery and XP are not touched here:
// the screen records the answer like any quiz answer.
bool examq_answer(Progress &p, const std::string &date, const ExamQ &q, int choice, bool &correct, int &bytesWon, std::string &why);

// The spoiler-free line to send a friend: "Exam Q #3: right (4 day streak)".
std::string examq_share(const Progress &p, const std::string &date);
