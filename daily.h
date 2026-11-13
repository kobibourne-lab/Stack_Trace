// daily.h - the Duolingo-style loop: the streak, const freezes and daily quests.
//
// The streak: learn something every day (answer a question, finish a lesson,
// solve a problem, beat a boss) and the flame keeps burning. Miss a day and it
// goes out, unless you've got a const freeze: one is used up for each missed
// day, automatically, the next time the app opens. Freezes are bought with
// bytes in the shop ("const int streak;" - for a day it can't change).
//
// Daily quests: three a day, picked from the date, so everyone gets the same
// three (handy for racing a friend). Each one finished opens a chest of bytes;
// finish all three and the golden chest opens too, sometimes with a free item
// from the wardrobe shop inside.
//
// No drawing here (screen_daily.cpp draws it), so it can be tested.

#pragma once
#include <string>
#include <vector>
#include "progress.h"

const int FREEZE_PRICE = 50;     // bytes for one const freeze
const int FREEZE_MAX = 2;        // the most you can hold
const int GOLDEN_BYTES = 30;     // the golden chest always has these
const int QUEST_COUNT = 3;
const int GOLDEN_CHEST = 3;      // daily_claim's index for the golden chest

struct Quest {
    std::string key;     // the counter it watches: "correct", "lesson", "game", "combo"...
    int target;          // how many it needs
    std::string text;    // "Get 5 answers right"
    int reward;          // bytes in its chest
    bool best;           // the counter is a best-of-the-day (a score), not a running total
};

// Start a new day's counters if the date has moved on.
void daily_roll(Progress &p, const std::string &today);

// Add to one of today's counters ("correct", "game"...).
void daily_add(Progress &p, const std::string &today, const std::string &key, int n = 1);
// Keep the best of the day ("combo", "snake"...).
void daily_best(Progress &p, const std::string &today, const std::string &key, int value);

// Today's three quests (the same for everyone on that date).
std::vector<Quest> daily_quests(const std::string &date);
int quest_progress(const Progress &p, const Quest &q, const std::string &today);   // capped at the target
bool quest_done(const Progress &p, const Quest &q, const std::string &today);
int quests_done(const Progress &p, const std::string &today);
bool chest_opened(const Progress &p, int index, const std::string &today);

// What's in the golden chest today: always GOLDEN_BYTES, and on some days a
// shop item you don't own yet (itemId, or "" for none).
struct GoldenPrize { int bytes = GOLDEN_BYTES; std::string itemId; };
GoldenPrize daily_golden(const Progress &p, const std::string &date);

// Open a chest: 0..2 a quest's, GOLDEN_CHEST the golden one. Adds what's in
// it and returns the bytes (and the item, for the golden one). Returns false
// with why if it can't be opened (not done yet, or already opened).
bool daily_claim(Progress &p, const std::string &today, int index, int &bytes, std::string &itemId, std::string &why);

// Mark today as a day something was learned. Returns true if that's new
// today (the streak just went up: time for the flame).
bool streak_mark_active(Progress &p, const std::string &today);

// Use const freezes for the days missed since the last active day (call it
// when the app starts). Returns how many were used, 0 if none were needed or
// there weren't enough to cover the gap (the streak has gone out; they're kept).
int streak_use_freezes(Progress &p, const std::string &today);

// Buy a const freeze. Returns false with why if you can't.
bool streak_buy_freeze(Progress &p, std::string &why);

// The last 7 days, oldest first, for the week row: 0 nothing, 1 learned, 2 frozen.
std::vector<int> streak_week(const Progress &p, const std::string &today);
