// apptheme.h - the app's themes: the whole look of Stack Trace, not just Mal.
// Five of them: the normal dark look, and four to unlock: Phosphor (an old
// green-screen terminal, unlocked at level 5), Neon Night (synthwave, bought
// for bytes), Paper (a light notebook look, free) and Blueprint (an engineer's
// drawing, for beating 3 campaign bosses). The one in use is saved (THEME).
//
// No drawing here (theme.cpp draws them), so it can be tested.

#pragma once
#include <string>
#include <vector>
#include "progress.h"

struct AppTheme {
    std::string id;      // "phosphor"; saved as THEME: phosphor, bought as OWNED: theme_phosphor
    std::string name;    // "Phosphor"
    std::string blurb;   // a line for the shop
    int level = 0;       // reach this level to unlock it (0 = no level needed)
    int bosses = 0;      // beat this many campaign bosses to unlock it
    int price = 0;       // bytes to buy it (0 = free once the rest is done)
};

const std::vector<AppTheme> &app_themes();
const AppTheme *app_theme_find(const std::string &id);

// Done what it takes (the level, the bosses), not counting the price.
bool app_theme_earned(const Progress &p, const AppTheme &t);
// Can be used now: earned, and bought if it has a price.
bool app_theme_unlocked(const Progress &p, const AppTheme &t);
// What unlocks it, for the shop ("Reach level 5", "Beat 3 bosses").
std::string app_theme_requirement(const AppTheme &t);

// Buy one that has a price. Returns false with why if it can't be bought.
bool app_theme_buy(Progress &p, const std::string &id, std::string &why);
// Switch to one. Returns false with why if it's still locked.
bool app_theme_use(Progress &p, const std::string &id, std::string &why);
