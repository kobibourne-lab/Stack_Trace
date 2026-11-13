// wardrobe.h - what Mal can wear, and what unlocks each item. Hats, skins
// and pets unlock with XP levels, beaten bosses, mastering topics and
// crashing in a MAL-OS game, so progress has something to show off. What Mal
// is wearing is saved in progress.txt (the WEAR line).
//
// Items with a price cost bytes to wear. Level items unlock with your level
// and then have to be bought; shop-only items (a price but no level) are in
// the wardrobe's SHOP tab. Boss and mastery rewards are free: they're
// achievements. What's been bought is saved as the OWNED line.
//
// No drawing here (mascot.cpp draws the items), so it can be tested.

#pragma once
#include <string>
#include <vector>
#include <set>
#include "progress.h"

enum class WearKind { Hat, Skin, Pet };

struct WardrobeItem {
    std::string id;        // saved in progress.txt, e.g. "partyhat"
    std::string name;      // shown in the wardrobe, e.g. "Party hat"
    WearKind kind;
    int level;             // XP level needed (0 = none)
    int bosses;            // bosses beaten needed (0 = none)
    bool allMastered;      // needs every topic Mastered
    int crashes = 0;       // game overs in MAL-OS games needed (0 = none)
    int price = 0;         // bytes, for shop items (0 = not in the shop)
};

// Everything needed to decide what's unlocked.
struct UnlockInfo {
    int level = 1;
    int bossesBeaten = 0;
    int topicsMastered = 0, topicsTotal = 0;
    int crashes = 0;          // game overs in MAL-OS games
    bool unlockAll = false;   // MAL-OS's "sudo unlock-all", for demos
    std::set<std::string> owned;   // shop items bought (Progress::owned)
};

const std::vector<WardrobeItem> &wardrobe_items();
const WardrobeItem *wardrobe_find(const std::string &id);

// The id an item has now (some were renamed), so older saves still load.
std::string wardrobe_rename(const std::string &id);
// Earned: its level, bosses or mastery have been reached (it may still need buying).
bool wardrobe_earned(const WardrobeItem &item, const UnlockInfo &u);
// Unlocked: Mal can wear it now (earned, and bought if it has a price).
bool wardrobe_unlocked(const WardrobeItem &item, const UnlockInfo &u);

// What still has to happen to unlock it, e.g. "Reach level 5" or "60 bytes".
std::string wardrobe_requirement(const WardrobeItem &item);

// Buy an item: takes its price off the bytes and adds it to what's owned.
// Returns false, with why, if it can't (not for sale, already owned, not
// earned yet, too few bytes).
bool wardrobe_buy(Progress &p, const WardrobeItem &item, const UnlockInfo &u, std::string &why);
