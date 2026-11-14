// farm.h - the Memory Farm, through the door in Mal's room. Every topic is a
// crop: it grows with the topic's mastery (seed, sprout, plant, tall plant,
// in fruit), and wilts if you haven't answered a question on it for a while,
// so the farm shows what needs reviewing (spaced repetition, Duolingo style).
// A quiz on a wilting crop waters it. Crops in fruit can be harvested for
// bytes every couple of days; the drone (bought with bytes) does it for you.
// The code tree stands in the middle of the farm.
//
// No drawing here (screen_farm.cpp draws it), so it can be tested.

#pragma once
#include <string>
#include "progress.h"

const int DRONE_PRICE = 150;      // bytes
const int HARVEST_DAYS = 2;       // days between harvests of one crop

// 0 seed (not started), 1 sprout (attempted), 2 plant (familiar),
// 3 tall plant (proficient), 4 in fruit (mastered).
int crop_stage(const Progress &p, const std::string &topic);

// The last day a question on the topic was answered ("" if never).
std::string crop_last_review(const Progress &p, const std::string &topic);

// Days a crop stays fresh after a review: longer the better it's known.
int crop_fresh_days(int stage);

// Wilting: started, and not reviewed for longer than its fresh days.
bool crop_wilting(const Progress &p, const std::string &topic, const std::string &today);
// Days until it starts to wilt (0 = wilting now; -1 for a seed, which never wilts).
int crop_days_left(const Progress &p, const std::string &topic, const std::string &today);

// The bytes a harvest gives (0 if it can't be harvested now): a tall plant 4,
// one in fruit 8, once every HARVEST_DAYS, and never while it's wilting.
int crop_harvest_value(const Progress &p, const std::string &topic, const std::string &today);
// Harvest it: adds the bytes and remembers the day. Returns the bytes (0 = not ready).
int crop_harvest(Progress &p, const std::string &topic, const std::string &today);

bool farm_has_drone(const Progress &p);
bool farm_buy_drone(Progress &p, std::string &why);
