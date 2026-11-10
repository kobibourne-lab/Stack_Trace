// wardrobe.cpp - Mal's hats and colours, and their unlock rules.

#include "wardrobe.h"

const std::vector<WardrobeItem> &wardrobe_items()
{
    static const std::vector<WardrobeItem> items = {
        // Hats (and things worn on the head)
        { "none",       "No hat",          WearKind::Hat,  0,  0, false },
        { "partyhat",   "Party hat",       WearKind::Hat,  2,  0, false },
        { "shades",     "Sunglasses",      WearKind::Hat,  3,  0, false },
        { "hardhat",    "Hard hat",        WearKind::Hat,  5,  0, false },
        { "headphones", "Headphones",      WearKind::Hat,  7,  0, false },
        { "bowtie",     "Bow tie",         WearKind::Hat,  0,  1, false },
        { "tophat",     "Top hat",         WearKind::Hat,  0,  2, false },
        { "wizard",     "Wizard hat",      WearKind::Hat,  12, 0, false },
        { "crown",      "Crown",           WearKind::Hat,  0,  0, true  },
        // Colours
        { "classic",    "Classic green",   WearKind::Skin, 0,  0, false },
        { "amber",      "Amber",           WearKind::Skin, 4,  0, false },
        { "purple",     "Heap purple",     WearKind::Skin, 6,  0, false },
        { "blue",       "Pointer blue",    WearKind::Skin, 8,  0, false },
        { "coral",      "Boss slayer red", WearKind::Skin, 0,  4, false },
        { "gold",       "Solid gold",      WearKind::Skin, 0,  0, true  },
    };
    return items;
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
    if (item.allMastered && (u.topicsTotal == 0 || u.topicsMastered < u.topicsTotal)) return false;
    return true;
}

std::string wardrobe_requirement(const WardrobeItem &item)
{
    if (item.allMastered) return "Master all topics";
    if (item.bosses > 0)
        return "Beat " + std::to_string(item.bosses) + (item.bosses == 1 ? " boss" : " bosses");
    if (item.level > 0) return "Reach level " + std::to_string(item.level);
    return "Always yours";
}
