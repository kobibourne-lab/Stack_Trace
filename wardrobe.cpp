// wardrobe.cpp - Mal's hats, skins and pets, and their unlock rules.

#include "wardrobe.h"

const std::vector<WardrobeItem> &wardrobe_items()
{
    static const std::vector<WardrobeItem> items = {
        // Hats and hair, in the order they unlock: levels, then bosses, then mastery
        { "none",       "No hat",          WearKind::Hat,  0,  0, false },
        { "drinkhat",   "Drink hat",       WearKind::Hat,  2,  0, false },   // two cans and straws
        { "shades",     "Sunglasses",      WearKind::Hat,  3,  0, false },
        { "hardhat",    "Hard hat",        WearKind::Hat,  5,  0, false },
        { "afro",       "Brace afro",      WearKind::Hat,  6,  0, false },   // a big round afro of { }
        { "gradcap",    "Grad cap",        WearKind::Hat,  7,  0, false },
        { "mohawk",     "Semicolon hawk",  WearKind::Hat,  9,  0, false },   // upside-down ; spikes
        { "wizard",     "Wizard hat",      WearKind::Hat,  12, 0, false },
        { "bowtie",     "Bow tie",         WearKind::Hat,  0,  1, false },
        { "helicopter", "Helicopter hat",  WearKind::Hat,  0,  2, false },   // a propeller beanie
        { "crown",      "Crown",           WearKind::Hat,  0,  0, true  },
        // Skins, in the order they unlock
        { "classic",    "Classic green",   WearKind::Skin, 0,  0, false },
        { "glitch",     "Glitch",          WearKind::Skin, 0,  0, false, 1 },   // corrupted memory
        { "amber",      "Amber",           WearKind::Skin, 4,  0, false },
        { "purple",     "Heap purple",     WearKind::Skin, 6,  0, false },
        { "blue",       "Pointer blue",    WearKind::Skin, 8,  0, false },
        { "tux",        "Tuxedo",          WearKind::Skin, 10, 0, false },
        // Boss skins: beat a boss and you can wear its look
        { "offbyone",   "The Off-By-One",  WearKind::Skin, 0,  1, false },   // an extra eye, 0x9001
        { "dangling",   "Dangling Pointer",WearKind::Skin, 0,  2, false },   // half freed, flickers
        { "leaker",     "The Leaker",      WearKind::Skin, 0,  3, false },   // drips leaked memory
        { "shallow",    "Shallow Copy",    WearKind::Skin, 0,  4, false },   // a ghost copy, one array
        { "slayer",     "Boss slayer gold",WearKind::Skin, 0,  4, false },
        { "diamond",    "Diamond",         WearKind::Skin, 0,  0, true  },
        // Pets: follow Mal around
        { "nopet",      "No pet",          WearKind::Pet,  0,  0, false },
        { "cat",        "const cat",       WearKind::Pet,  4,  0, false },   // never moves
        { "dog",        "static dog",      WearKind::Pet,  8,  0, false },   // remembers you
        { "bird",       "volatile bird",   WearKind::Pet,  10, 0, false },   // changes when you're not looking
    };
    return items;
}

std::string wardrobe_rename(const std::string &id)
{
    // Items that were renamed, so older saves keep what Mal was wearing
    if (id == "partyhat") return "drinkhat";
    if (id == "tophat") return "helicopter";
    if (id == "headphones") return "gradcap";
    if (id == "coral") return "slayer";
    if (id == "gold") return "diamond";
    return id;
}

const WardrobeItem *wardrobe_find(const std::string &id)
{
    for (const WardrobeItem &it : wardrobe_items())
        if (it.id == id) return &it;
    return nullptr;
}

bool wardrobe_unlocked(const WardrobeItem &item, const UnlockInfo &u)
{
    if (u.unlockAll) return true;
    if (u.level < item.level) return false;
    if (u.bossesBeaten < item.bosses) return false;
    if (u.crashes < item.crashes) return false;
    if (item.allMastered && (u.topicsTotal == 0 || u.topicsMastered < u.topicsTotal)) return false;
    return true;
}

std::string wardrobe_requirement(const WardrobeItem &item)
{
    if (item.allMastered) return "Master all topics";
    if (item.crashes > 0) return "Crash a MAL-OS game";
    if (item.bosses > 0)
        return "Beat " + std::to_string(item.bosses) + (item.bosses == 1 ? " boss" : " bosses");
    if (item.level > 0) return "Reach level " + std::to_string(item.level);
    return "Always yours";
}
