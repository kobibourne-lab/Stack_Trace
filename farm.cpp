// farm.cpp - the Memory Farm's crops (see farm.h).

#include "farm.h"

int crop_stage(const Progress &p, const std::string &topic)
{
    switch (topic_mastery(p, topic)) {
        case Mastery::Mastered: return 4;
        case Mastery::Proficient: return 3;
        case Mastery::Familiar: return 2;
        case Mastery::Attempted: return 1;
        default: return 0;
    }
}

std::string crop_last_review(const Progress &p, const std::string &topic)
{
    std::string last;
    for (const AnswerRecord &a : p.answers)
        if (a.topic == topic && a.date > last) last = a.date;
    return last;
}

int crop_fresh_days(int stage)
{
    static const int days[] = { 0, 2, 3, 5, 8 };
    return days[stage < 0 ? 0 : stage > 4 ? 4 : stage];
}

int crop_days_left(const Progress &p, const std::string &topic, const std::string &today)
{
    int stage = crop_stage(p, topic);
    std::string last = crop_last_review(p, topic);
    if (stage == 0) return -1;
    if (last.empty()) return 0;   // mastered without a single answer logged: needs a look
    int since = date_to_days(today) - date_to_days(last);
    int left = crop_fresh_days(stage) - since;
    return left < 0 ? 0 : left;
}

bool crop_wilting(const Progress &p, const std::string &topic, const std::string &today)
{
    return crop_days_left(p, topic, today) == 0;
}

int crop_harvest_value(const Progress &p, const std::string &topic, const std::string &today)
{
    int stage = crop_stage(p, topic);
    if (stage < 3 || crop_wilting(p, topic, today)) return 0;
    auto it = p.harvested.find(topic);
    if (it != p.harvested.end() && date_to_days(today) - date_to_days(it->second) < HARVEST_DAYS) return 0;
    return stage == 4 ? 8 : 4;
}

int crop_harvest(Progress &p, const std::string &topic, const std::string &today)
{
    int bytes = crop_harvest_value(p, topic, today);
    if (bytes == 0) return 0;
    p.bytes += bytes;
    p.harvested[topic] = today;
    return bytes;
}

bool farm_has_drone(const Progress &p)
{
    return p.owned.count("drone") > 0;
}

bool farm_buy_drone(Progress &p, std::string &why)
{
    if (farm_has_drone(p)) { why = "You've already got one"; return false; }
    if (p.bytes < DRONE_PRICE) { why = "Need " + std::to_string(DRONE_PRICE - p.bytes) + " more bytes"; return false; }
    p.bytes -= DRONE_PRICE;
    p.owned.insert("drone");
    return true;
}
