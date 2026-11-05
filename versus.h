// versus.h - the rules of a Versus match, and the messages the two copies of
// Stack Trace send each other. No drawing and no networking in here:
// screen_versus.cpp sends and receives the lines (net.cpp) and draws the match;
// this file decides what each line means. That keeps it testable.
//
// The messages, one per line (the host is the one who pressed Host):
//   HELLO 1 <name>                    both, on connecting (1 = the message version)
//   SETTINGS <set> <difficulty> <min> host: what the next match will be, as they change
//   START <set> <problem> <min>       host: start the countdown for this problem
//   MISSING <problem>                 joiner: that problem isn't in my copy
//   RUNNING                           I've pressed Run or Submit
//   RAN <passed> <total> <result>     the result: passed, wrong, compile, crash, timeout, rule
//   DONE <seconds>                    accepted, this long after the start
//   OVER                              out of time, and my last run has been sent
//   LINES <n>                         how many lines my code has (every couple of seconds)
//   CODE <code>                       my code at the end (\n written as \\n)
//   LEFT                              I've left this match
//   REMATCH                           back to the lobby for another
//   BYE                               I'm closing the connection

#pragma once
#include <string>
#include <vector>
#include "practice.h"

const int VERSUS_VERSION = 1;
const double COUNTDOWN_SECONDS = 3.0;
const double END_GRACE_SECONDS = 15.0;   // after time's up, how long to wait for the other's last result

enum class VsPhase { Lobby, Countdown, Playing, Finished };

struct VsPlayer {
    std::string name;
    int passed = 0, total = 0;      // the last run
    int bestPassed = 0;             // the most tests passed in any run
    std::string lastResult;         // "wrong", "compile"... ("" before the first run)
    int runs = 0, compileErrors = 0;
    bool running = false;           // a run is going on right now
    bool finished = false;          // accepted
    double finishSeconds = 0;
    bool over = false;              // finished, or out of time with the last run done
    bool left = false;
    int lines = 0;
    std::string code;
    bool hasCode = false;
};

struct VsEvent {
    double seconds;                 // since the match started
    std::string text;
    int kind;                       // 0 info, 1 bad, 2 some tests passing, 3 accepted
};

struct VsMatch {
    bool host = false;
    VsPhase phase = VsPhase::Lobby;
    bool helloReceived = false;
    std::string problemId, setId;   // the match's problem
    int minutes = 15;
    double startAt = 0;             // when the clock starts (after the countdown)
    double endedAt = 0;
    VsPlayer me, them;
    std::vector<VsEvent> feed;      // what the other player has done, newest last

    // The lobby (the host's choices; the joiner sees them through SETTINGS)
    std::string lobbySet, lobbyDifficulty = "Any";
    int lobbyMinutes = 15;

    // Set by vs_apply for the screen to act on
    bool startRequested = false;    // a START arrived: open problemId and count down
    std::string notice;             // something to show, e.g. "Kobi doesn't have that problem"

    int myWins = 0, theirWins = 0, draws = 0;   // this session's head-to-head
    bool scored = false;            // this match has been counted
};

// Parts of a received line: the first word, then the rest split by spaces.
struct VsMessage {
    std::string kind;
    std::vector<std::string> args;
    std::string rest;               // everything after the first word, as it was
};
VsMessage vs_parse(const std::string &line);

std::string vs_escape(const std::string &code);     // newlines become \n, so code fits on one line
std::string vs_unescape(const std::string &text);

// What a run's result is called in a RAN message.
std::string vs_result_word(const RunResult &r);
// And how it reads in the feed: "compile error", "3 / 4 tests"...
std::string vs_result_text(const std::string &word, int passed, int total);

// Handles one line from the other player. now is the time in seconds.
void vs_apply(VsMatch &m, const std::string &line, double now);

// Starts a fresh match for problem (both players reset, countdown from now).
void vs_begin(VsMatch &m, const std::string &setId, const std::string &problemId, int minutes, double now);

// Records one of my runs; returns the lines to send the other player.
std::vector<std::string> vs_my_run(VsMatch &m, const RunResult &r, bool submitted, double now);

// Time's up and my last run is in: returns the line to send (OVER).
std::string vs_my_time_up(VsMatch &m);

// Has the match finished? (both done, time's up and both over, someone left,
// or the grace period after time's up has passed). Moves the phase on and
// scores it once.
bool vs_check_end(VsMatch &m, double now);

// The result: 1 I won, -1 they won, 0 a draw, with the reason in words.
int vs_outcome(const VsMatch &m, std::string &reason);

double vs_elapsed(const VsMatch &m, double now);     // seconds since the start (0 in the countdown)
double vs_remaining(const VsMatch &m, double now);   // seconds left on the clock

// The computer's user name with a capital letter, as a starting name.
std::string vs_default_name();
