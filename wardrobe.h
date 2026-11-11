// wardrobe.h - what Mal can wear, and what unlocks each item. Hats, skins
// and pets unlock with XP levels, beaten bosses, mastering topics and
// crashing in a MAL-OS game, so progress has something to show off. What Mal
// is wearing is saved in progress.txt (the WEAR line).
//
// No drawing here (mascot.cpp draws the items), so it can be tested.

#pragma once
#include <string>
#include <vector>

enum class WearKind { Hat, Skin, Pet };

struct WardrobeItem {
    std::string id;        // saved in progress.txt, e.g. "partyhat"
    std::string name;      // shown in the wardrobe, e.g. "Party hat"
    WearKind kind;
    int level;             // XP level needed (0 = none)
    int bosses;            // bosses beaten needed (0 = none)
    bool allMastered;      // needs every topic Mastered
    int crashes = 0;       // game overs in MAL-OS games needed (0 = none)
};

// Everything needed to decide what's unlocked.
struct UnlockInfo {
    int level = 1;
    int bossesBeaten = 0;
    int topicsMastered = 0, topicsTotal = 0;
    int crashes = 0;          // game overs in MAL-OS games
    bool unlockAll = false;   // MAL-OS's "sudo unlock-all", for demos
};

const std::vector<WardrobeItem> &wardrobe_items();
const WardrobeItem *wardrobe_find(const std::string &id);

// The id an item has now (some were renamed), so older saves still load.
std::string wardrobe_rename(const std::string &id);
bool wardrobe_unlocked(const WardrobeItem &item, const UnlockInfo &u);

// What still has to happen to unlock it, e.g. "Reach level 5".
std::string wardrobe_requirement(const WardrobeItem &item);
