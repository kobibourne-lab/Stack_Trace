// campaign.h - the campaign: regions of the machine's memory, each a run of
// topics and then a boss (an Exam prep problem played under exam conditions).
// Loaded from content/campaign.txt. This file decides what's cleared, what's
// open and how a boss fight is scored; screen_campaign.cpp and screen_boss.cpp
// draw it.
//
// No drawing in here (no raylib), so tests/test_core.cpp can test it.

#pragma once
#include <string>
#include <vector>
#include "progress.h"
#include "practice.h"

struct Region {
    std::string id;                  // "stack": saved in progress.txt when its boss is beaten
    std::string name;                // "The Stack"
    std::string covers;              // "C basics"
    std::string exam;                // "Exam 1", "Exam 2" or ""
    std::vector<std::string> topics; // topic ids, in the order they're played
    std::string bossName;            // "The Off-By-One"
    std::string bossProblem;         // problem id, e.g. "lab1/q15_reverse_array"
    std::string bossLook;            // how the boss is drawn: offbyone, dangling, leaker, shallow
    std::string taunt, story, win;   // the lines said before the fight, on the map, after winning
};

bool load_campaign(const std::string &path, std::vector<Region> &regions, std::string &error);
bool parse_campaign(const std::string &text, std::vector<Region> &regions, std::string &error);

// ---------------------------------------------------------------------------
// Where the student is
// ---------------------------------------------------------------------------

// A topic is cleared once it's Familiar (50 points) or better.
bool topic_cleared(const Progress &p, const std::string &topic);

// The first region is always open; each one after opens when the boss before it is beaten.
bool region_open(const std::vector<Region> &regions, const Progress &p, int region);
bool region_beaten(const std::vector<Region> &regions, const Progress &p, int region);
int region_topics_left(const Region &r, const Progress &p);

// The region being played: the first open one whose boss isn't beaten yet
// (or the last region, once everything is beaten).
int current_region(const std::vector<Region> &regions, const Progress &p);

// The next topic to play in a region (an index into r.topics), or -1 when
// every topic is cleared and only the boss is left.
int next_topic(const Region &r, const Progress &p);

// ---------------------------------------------------------------------------
// Boss fights
// ---------------------------------------------------------------------------

const int BOSS_LIVES = 3;      // a wrong submit costs one

// How many tests a problem has (its HP), counted from the check calls in its
// tests, so the HP bar is right before the first run.
int count_tests(const Problem &p);

// The boss's HP left: its tests, minus the most any run has passed.
int boss_hp(int totalTests, int bestPassed);

// Stars for a win: one for winning, one for not losing a life, one for
// finishing in under half the time. 0 if it wasn't won.
int boss_stars(bool won, int livesLost, double seconds, double limitSeconds);
