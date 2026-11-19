// apptheme.cpp - the app's themes and what unlocks them (see apptheme.h).

#include "apptheme.h"

const std::vector<AppTheme> &app_themes()
{
    static const std::vector<AppTheme> themes = {
        { "default",   "Stack Trace", "The normal dark look.",                                      0, 0, 0 },
        { "phosphor",  "Phosphor",    "An old green-screen terminal: scanlines and falling hex.",   5, 0, 0 },
        { "neon",      "Neon Night",  "Synthwave: a sunset, a neon grid and glowing panels.",       0, 0, 300 },
        { "paper",     "Paper",       "Light mode as a notebook, with panels taped on.",            0, 0, 0 },
        { "blueprint", "Blueprint",   "An engineer's drawing: grid paper and measurements.",        0, 3, 0 },
    };
    return themes;
}

const AppTheme *app_theme_find(const std::string &id)
{
    for (const AppTheme &t : app_themes())
        if (t.id == id) return &t;
    return nullptr;
}

bool app_theme_earned(const Progress &p, const AppTheme &t)
{
    if (t.level > 0 && level_for_xp(progress_xp(p)) < t.level) return false;
    int beaten = 0;
    for (const auto &b : p.bosses) if (progress_boss_beaten(p, b.first)) beaten++;
    return beaten >= t.bosses;
}

bool app_theme_unlocked(const Progress &p, const AppTheme &t)
{
    if (!app_theme_earned(p, t)) return false;
    return t.price == 0 || p.owned.count("theme_" + t.id) > 0;
}

std::string app_theme_requirement(const AppTheme &t)
{
    if (t.level > 0) return "Reach level " + std::to_string(t.level);
    if (t.bosses > 0) return "Beat " + std::to_string(t.bosses) + " bosses";
    if (t.price > 0) return std::to_string(t.price) + " bytes";
    return "Free";
}

bool app_theme_buy(Progress &p, const std::string &id, std::string &why)
{
    const AppTheme *t = app_theme_find(id);
    if (!t) { why = "No such theme"; return false; }
    if (app_theme_unlocked(p, *t)) { why = "You've already got it"; return false; }
    if (!app_theme_earned(p, *t)) { why = app_theme_requirement(*t) + " first"; return false; }
    if (p.bytes < t->price) { why = "Need " + std::to_string(t->price - p.bytes) + " more bytes"; return false; }
    p.bytes -= t->price;
    p.owned.insert("theme_" + t->id);
    return true;
}

bool app_theme_use(Progress &p, const std::string &id, std::string &why)
{
    const AppTheme *t = app_theme_find(id);
    if (!t) { why = "No such theme"; return false; }
    if (!app_theme_unlocked(p, *t)) { why = app_theme_requirement(*t) + " to unlock it"; return false; }
    p.theme = id;
    return true;
}
