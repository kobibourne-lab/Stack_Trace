// codex.h - the C Codex: a reference you collect, like W3Schools crossed with
// a Pokedex. Each entry (printf, malloc, ->, new[] ...) unlocks once you've
// started its topic, and gets stars as the topic's mastery grows. Every entry
// has what it does, a whole program to try (edit it and run it, right there),
// what it prints, and the mistakes people make with it.
//
// No drawing here (screen_codex.cpp draws it), so it can be tested.

#pragma once
#include <string>
#include <vector>
#include "progress.h"

struct CodexEntry {
    std::string id;          // "malloc"
    std::string name;        // shown on the card: "malloc"
    std::string tag;         // "stdlib.h", "operator", "keyword", "C++"
    std::string topic;       // the topic that unlocks it
    std::string signature;   // "void *malloc(size_t size);"
    std::string about;       // what it does, a sentence or two
    std::string example;     // a whole program, ready to run
    std::string output;      // what the example prints
    std::vector<std::string> mistakes;   // "forgetting free(p) -> a leak"
    bool cpp = false;        // compile the example as C++
};

const std::vector<CodexEntry> &codex_entries();

// Unlocked once its topic has been started (anything other than Not started).
bool codex_unlocked(const Progress &p, const CodexEntry &e);
// 0 to 3 stars: Familiar, Proficient, Mastered.
int codex_stars(const Progress &p, const CodexEntry &e);
int codex_found(const Progress &p);

// Entries whose name, tag or topic contains the search (any case).
std::vector<int> codex_search(const std::string &text);
