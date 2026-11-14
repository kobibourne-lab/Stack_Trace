// bugdex.h - the Bugdex: a Pokedex of C bugs. Every bug has a type, a habitat
// (where it hides), a weakness (how to beat it) and the line it lives on.
//
// You catch a bug by fixing it:
//   - in its three quick fixes (spot the buggy line in a short program),
//   - by beating the campaign boss it belongs to (The Off-By-One, The Leaker...),
//   - or in Exam prep: a Segfault when a program that crashed gets accepted,
//     an Infinite Loop when one that timed out does.
// Catching one again adds to its count.
//
// No drawing here (screen_bugdex.cpp draws the bugs), so it can be tested.

#pragma once
#include <string>
#include <vector>
#include "progress.h"

enum class BugShape { Blob, Block, Ghost, Bolt, Loop, Drop };

struct BugFix {
    std::vector<std::string> code;   // a short program with the bug in it
    int line;                        // the buggy line (1 = first)
    std::string why;                 // what's wrong, and the fix
};

struct Bug {
    std::string id;          // "offbyone"
    std::string name;        // "Off-By-One"
    std::string type;        // "array type"
    std::string habitat;     // where it hides
    std::string weakness;    // how to beat it
    std::string bad, good;   // the line with it, and fixed
    std::string boss;        // the campaign boss it belongs to ("" for none)
    BugShape shape;
    unsigned int colour;     // 0xRRGGBB, for drawing it
    std::vector<BugFix> fixes;   // three quick fixes
};

const std::vector<Bug> &bugdex_bugs();
const Bug *bugdex_find(const std::string &id);

bool bug_caught(const Progress &p, const std::string &id);
int bugs_caught(const Progress &p);

// Catch a bug (or catch it again). how: "practice", "boss", "exam prep".
// Returns true if it's the first time.
bool bugdex_catch(Progress &p, const std::string &id, const std::string &date, const std::string &how);

// A quick fix answered: if line is the buggy one, it's marked done. When all
// three are done the bug is caught. Returns true if the answer was right;
// caughtNow says whether that catch just happened.
bool bugdex_answer_fix(Progress &p, const std::string &id, int fix, int line, const std::string &date, bool &caughtNow);
bool bugdex_fix_done(const Progress &p, const std::string &id, int fix);

// The bug a campaign boss belongs to ("" if none), by its boss id.
std::string bugdex_for_boss(const std::string &bossId);
