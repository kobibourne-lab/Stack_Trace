// dailyq.h - the Daily question: one Exam prep problem a day, picked from ALL
// the problems (every lab and exam set, not just one lab), the same for everyone
// on that date, like Codle. You write the code and Submit it like any Exam prep
// problem; solving today's pays bytes (more for a streak of days in a row).
// There's no limit on tries: it's open until midnight, then there's a new one.
//
// The day's pick comes from shuffling the whole pool once per cycle, so no
// problem comes round twice until every one has been used.
//
// No drawing here (screen_dailyq.cpp draws it), so it can be tested.

#pragma once
#include <string>
#include <vector>
#include "practice.h"
#include "progress.h"

const std::string DAILYQ_START = "2026-11-20";   // question #1
const int DAILYQ_BYTES = 30;          // solving today's problem
const int DAILYQ_STREAK_BONUS = 5;    // extra for each day already in the streak...
const int DAILYQ_STREAK_MAX = 5;      // ...up to this many days of it

// A problem in the pool: where it is in App::practice.
struct DailyRef {
    int set = 0, problem = 0;
};

// Every problem that Exam prep shows (the campaign's boss problems are left
// out), in the order of the sets.
std::vector<DailyRef> dailyq_pool(const std::vector<ProblemSet> &sets);

// Which problem (an index into the pool) that date has. The same for
// everyone, and no repeats until the whole pool has been used.
int dailyq_index(int poolSize, const std::string &date);

// Its number: #1 on DAILYQ_START.
int dailyq_number(const std::string &date);

// Was today's problem solved today?
bool dailyq_solved(const Progress &p, const std::string &date);

// Days in a row solved. Still alive today if yesterday was solved and today
// isn't yet; a missed day ends it.
int dailyq_streak(const Progress &p, const std::string &today);

// The bytes solving it pays. streakAfter is the streak once this one counts.
int dailyq_reward(int streakAfter);

// Today's problem was solved (in `seconds`): marks it, updates the stats and
// streak, and pays bytes (bytesWon says how many). Returns false with why if
// today's was already solved. It's only paid once a day, however often it's solved.
bool dailyq_solve(Progress &p, const std::string &date, int seconds, int &bytesWon, std::string &why);

// The spoiler-free line to send a friend: "Daily Q #3: solved in 4:12 (3 day streak)".
std::string dailyq_share(const Progress &p, const std::string &date);
