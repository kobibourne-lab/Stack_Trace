// mos.h - MAL-OS, the operating system on the computer in Mal's room. A
// pretend Unix shell: ls, cd, cat and friends over a small made-up file
// system (Mal's diary, boss intel, a hello.c to compile), plus programs in
// /bin that unlock as your XP level grows, a text adventure, and a few
// easter eggs for people who go poking.
//
// No drawing and no raylib here: the shell takes a line of input and adds
// lines of output, so it can be tested. screen_room.cpp draws it, and does the
// things the shell asks for (run a game, open the editor, call the compiler).

#pragma once
#include <map>
#include <string>
#include <vector>

enum class MosTone { Normal, Echo, Dim, Good, Bad, Lore, Accent };

struct MosLine {
    std::string text;
    MosTone tone = MosTone::Normal;
};

// What the screen should do after a command.
enum class MosAction { None, RunGame, Edit, Compile, RunProgram, Wardrobe, Exit };

struct MosResult {
    MosAction action = MosAction::None;
    std::string arg;   // the game to run, or the file to edit or compile
};

// Everything about the student that the shell needs to know.
struct MosInfo {
    std::string user = "student";
    int level = 1, xp = 0;
    int bossesBeaten = 0;
    std::vector<std::string> bossNames;   // in campaign order
    int topicsMastered = 0, topicsTotal = 0;
    std::map<std::string, int> scores;    // minigame -> best
    std::string today = "2026-01-01";
};

// The adventure (run dungeon): you're a pointer exploring memory.
struct Dungeon {
    std::string room = "main";
    std::vector<std::string> trail;   // rooms visited, so "return" goes back (like a call stack)
    bool hasKey = false, freed = false, won = false;
    int moves = 0;
};

struct Mos {
    std::vector<MosLine> out;
    std::string cwd = "/home/mal";
    std::vector<std::string> history;
    std::string helloSource;          // hello.c: edited with nano, saved to disk by the screen
    bool compiled = false;            // gcc has made a.out
    bool unlockAll = false;           // "sudo unlock-all": every program and wardrobe item
    bool inDungeon = false;
    Dungeon dungeon;
    int malQuote = 0;
};

// The level each program in /bin unlocks at.
struct MosProgram { std::string name, title, about; int level; };
const std::vector<MosProgram> &mos_programs();
bool mos_program_unlocked(const Mos &m, const MosInfo &info, const MosProgram &p);

// Clears the screen and shows the boot messages.
void mos_boot(Mos &m, const MosInfo &info);

// The prompt shown before the input, e.g. "kobi@mal-os:~$ " (or "> " in the dungeon).
std::string mos_prompt(const Mos &m, const MosInfo &info);

// Runs one line typed by the student.
MosResult mos_run(Mos &m, const MosInfo &info, const std::string &line);

// Tab completion: the line with its last word completed as far as it can be.
std::string mos_complete(const Mos &m, const MosInfo &info, const std::string &line);

// Adds a line of output (the screen uses this for compiler output).
void mos_print(Mos &m, const std::string &text, MosTone tone = MosTone::Normal);

// The hello.c every new MAL-OS starts with.
std::string mos_default_hello();
