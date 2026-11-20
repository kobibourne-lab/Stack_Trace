// dailyq.cpp - the Daily question (see dailyq.h).

#include "dailyq.h"
#include <random>

std::vector<DailyRef> dailyq_pool(const std::vector<ProblemSet> &sets)
{
    std::vector<DailyRef> pool;
    int shown = shown_set_count(sets);
    for (int s = 0; s < shown; s++)
        for (int i = 0; i < (int)sets[s].problems.size(); i++) pool.push_back({ s, i });
    return pool;
}

// The pool in a shuffled order that belongs to one cycle (cycle 0, 1, 2...).
// mt19937 gives the same numbers on every computer, so everyone gets the same day.
static std::vector<int> cycle_order(int poolSize, int cycle)
{
    std::vector<int> order(poolSize);
    for (int i = 0; i < poolSize; i++) order[i] = i;
    std::mt19937 rng(2654435761u * (unsigned int)(cycle + 1) + 12345u);
    for (int i = poolSize - 1; i > 0; i--) std::swap(order[i], order[rng() % (unsigned int)(i + 1)]);
    return order;
}

int dailyq_index(int poolSize, const std::string &date)
{
    int day = date_to_days(date);
    if (poolSize <= 0 || day < 0) return 0;
    int cycle = day / poolSize, at = day % poolSize;
    std::vector<int> order = cycle_order(poolSize, cycle);
    // The first problem of a cycle shouldn't be the last of the one before
    if (cycle > 0 && poolSize > 2 && order[0] == cycle_order(poolSize, cycle - 1)[poolSize - 1]) std::swap(order[0], order[1]);
    return order[at];
}

int dailyq_number(const std::string &date)
{
    int n = date_to_days(date) - date_to_days(DAILYQ_START) + 1;
    return n < 1 ? 1 : n;
}

bool dailyq_solved(const Progress &p, const std::string &date)
{
    return p.dailyq.date == date && p.dailyq.solved;
}

int dailyq_streak(const Progress &p, const std::string &today)
{
    const DailyQStats &s = p.dailyqStats;
    if (s.streak <= 0 || s.lastSolved.empty()) return 0;
    if (s.lastSolved == today || s.lastSolved == date_add_days(today, -1)) return s.streak;
    return 0;
}

int dailyq_reward(int streakAfter)
{
    int extra = streakAfter - 1;
    if (extra < 0) extra = 0;
    if (extra > DAILYQ_STREAK_MAX) extra = DAILYQ_STREAK_MAX;
    return DAILYQ_BYTES + extra * DAILYQ_STREAK_BONUS;
}

bool dailyq_solve(Progress &p, const std::string &date, int seconds, int &bytesWon, std::string &why)
{
    bytesWon = 0;
    if (date_to_days(date) < 0) { why = "Not a date"; return false; }
    if (dailyq_solved(p, date)) { why = "Today's question is already solved"; return false; }

    p.dailyq.date = date;
    p.dailyq.solved = true;
    p.dailyq.seconds = seconds < 0 ? 0 : seconds;

    DailyQStats &s = p.dailyqStats;
    s.solved++;
    s.streak = (s.lastSolved == date_add_days(date, -1)) ? s.streak + 1 : 1;
    s.lastSolved = date;
    if (s.streak > s.best) s.best = s.streak;

    bytesWon = dailyq_reward(s.streak);
    p.bytes += bytesWon;
    return true;
}

std::string dailyq_share(const Progress &p, const std::string &date)
{
    std::string out = "Daily Q #" + std::to_string(dailyq_number(date)) + ": ";
    if (!dailyq_solved(p, date)) return out + "not solved yet";
    int s = p.dailyq.seconds;
    out += "solved in " + std::to_string(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + std::to_string(s % 60);
    int streak = dailyq_streak(p, date);
    if (streak > 1) out += " (" + std::to_string(streak) + " day streak)";
    return out;
}
