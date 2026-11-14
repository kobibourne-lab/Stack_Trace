// outputle.h - Outputle: Wordle with C. One snippet a day (the same for
// everyone), and six tries to guess exactly what it prints. Each guess is
// as long as the answer; a green square is the right character in the right
// place, yellow is in the answer somewhere else, grey isn't in it at all.
// Solving it pays bytes (more for fewer tries) and keeps an Outputle streak.
//
// No drawing here (screen_outputle.cpp draws it), so it can be tested.

#pragma once
#include <string>
#include <vector>
#include "progress.h"

const int OUTPUTLE_TRIES = 6;
const std::string OUTPUTLE_START = "2026-11-13";   // puzzle #1

struct OutPuzzle {
    std::vector<std::string> code;   // the snippet (inside main), a line each
    std::string answer;              // exactly what it prints: no spaces, no '|'
    std::string topic;               // topic id, shown as a hint
    std::string why;                 // the explanation shown at the end
};

const std::vector<OutPuzzle> &outputle_puzzles();
int outputle_index(const std::string &date);    // which puzzle that day has
int outputle_number(const std::string &date);   // its number, #1 on OUTPUTLE_START

// Colours for a guess: 2 green, 1 yellow, 0 grey (Wordle's rules for repeats:
// a character is only yellow as many times as it's still unmatched).
std::vector<int> outputle_grade(const std::string &guess, const std::string &answer);

// A guess has to be the answer's length, with no spaces or '|'.
bool outputle_valid(const std::string &guess, const std::string &answer, std::string &why);

const std::vector<std::string> &outputle_guesses(const Progress &p, const std::string &date);
bool outputle_won(const Progress &p, const std::string &date);
bool outputle_over(const Progress &p, const std::string &date);   // won, or out of tries

// The bytes a win pays: 35 for one try, down to 10 for six.
int outputle_reward(int tries);

// Make a guess at today's puzzle. Returns false with why if it isn't allowed
// (wrong length, already over). When the game ends, the stats and streak are
// updated and bytesWon is what a win paid (0 otherwise).
bool outputle_guess(Progress &p, const std::string &date, const std::string &guess, int &bytesWon, std::string &why);

// The spoiler-free result to paste to a friend: "Outputle #3 4/6" and the squares.
std::string outputle_share(const Progress &p, const std::string &date);
