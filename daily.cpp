// daily.cpp - the streak, const freezes and daily quests (see daily.h).

#include "daily.h"
#include "wardrobe.h"

// A number from the date, so each day picks its own quests (FNV-1a).
static unsigned int date_hash(const std::string &date, unsigned int salt)
{
    unsigned int h = 2166136261u ^ salt;
    for (char c : date) { h ^= (unsigned char)c; h *= 16777619u; }
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}

void daily_roll(Progress &p, const std::string &today)
{
    if (p.daily.date == today) return;
    p.daily = DailyLog();
    p.daily.date = today;
}

void daily_add(Progress &p, const std::string &today, const std::string &key, int n)
{
    daily_roll(p, today);
    p.daily.counts[key] += n;
}

void daily_best(Progress &p, const std::string &today, const std::string &key, int value)
{
    daily_roll(p, today);
    int &best = p.daily.counts[key];
    if (value > best) best = value;
}

// The quests, in three piles: one is picked from each, every day.
static const std::vector<Quest> &pile(int which)
{
    static const std::vector<Quest> learn = {
        { "correct", 5, "Get 5 answers right", 15, false },
        { "lesson", 1, "Finish a lesson", 15, false },
        { "run", 3, "Run your code 3 times in Exam prep", 15, false },
        { "answer", 8, "Answer 8 quiz questions", 15, false },
    };
    static const std::vector<Quest> push = {
        { "correct", 15, "Get 15 answers right", 25, false },
        { "combo", 5, "Get 5 right in a row in one quiz", 25, true },
        { "quiz", 2, "Pass 2 quizzes", 25, false },
        { "solve", 1, "Solve an Exam prep problem", 25, false },
    };
    static const std::vector<Quest> play = {
        { "game", 3, "Play 3 games on MAL-OS", 20, false },
        { "snake", 60, "Score 60 in Linked-list Snake", 20, true },
        { "heaphero", 100, "Score 100 in Heap Hero", 20, true },
        { "stacktower", 100, "Score 100 in Stack Tower", 20, true },
    };
    return which == 0 ? learn : which == 1 ? push : play;
}

std::vector<Quest> daily_quests(const std::string &date)
{
    std::vector<Quest> quests;
    for (int i = 0; i < QUEST_COUNT; i++) {
        const std::vector<Quest> &from = pile(i);
        quests.push_back(from[date_hash(date, 101u * (i + 1)) % from.size()]);
    }
    // Two "correct" quests on one day would finish together: swap the second
    if (quests[0].key == "correct" && quests[1].key == "correct") quests[1] = pile(1)[2];
    return quests;
}

int quest_progress(const Progress &p, const Quest &q, const std::string &today)
{
    if (p.daily.date != today) return 0;
    auto it = p.daily.counts.find(q.key);
    int n = it == p.daily.counts.end() ? 0 : it->second;
    return n > q.target ? q.target : n;
}

bool quest_done(const Progress &p, const Quest &q, const std::string &today)
{
    return quest_progress(p, q, today) >= q.target;
}

int quests_done(const Progress &p, const std::string &today)
{
    int n = 0;
    for (const Quest &q : daily_quests(today)) if (quest_done(p, q, today)) n++;
    return n;
}

bool chest_opened(const Progress &p, int index, const std::string &today)
{
    return p.daily.date == today && (p.daily.claimed & (1 << index)) != 0;
}

GoldenPrize daily_golden(const Progress &p, const std::string &date)
{
    GoldenPrize prize;
    if (date_hash(date, 7u) % 3 != 0) return prize;   // an item one day in three
    std::vector<std::string> notOwned;
    for (const WardrobeItem &item : wardrobe_items())
        if (item.price > 0 && item.level == 0 && !p.owned.count(item.id)) notOwned.push_back(item.id);
    if (!notOwned.empty()) prize.itemId = notOwned[date_hash(date, 13u) % notOwned.size()];
    return prize;
}

bool daily_claim(Progress &p, const std::string &today, int index, int &bytes, std::string &itemId, std::string &why)
{
    bytes = 0;
    itemId.clear();
    daily_roll(p, today);
    if (index < 0 || index > GOLDEN_CHEST) { why = "No such chest"; return false; }
    if (chest_opened(p, index, today)) { why = "Already opened"; return false; }
    std::vector<Quest> quests = daily_quests(today);
    if (index == GOLDEN_CHEST) {
        if (quests_done(p, today) < QUEST_COUNT) { why = "Finish all three quests first"; return false; }
        GoldenPrize prize = daily_golden(p, today);
        bytes = prize.bytes;
        itemId = prize.itemId;
        if (!itemId.empty()) p.owned.insert(itemId);
    } else {
        if (!quest_done(p, quests[index], today)) { why = "Quest not finished yet"; return false; }
        bytes = quests[index].reward;
    }
    p.bytes += bytes;
    p.daily.claimed |= 1 << index;
    return true;
}

bool streak_mark_active(Progress &p, const std::string &today)
{
    bool already = progress_day_active(p, today);
    p.activeDays.insert(today);
    return !already;
}

int streak_use_freezes(Progress &p, const std::string &today)
{
    // The last day something was learned, before today
    int now = date_to_days(today);
    if (now < 0) return 0;
    int last = -1;
    for (const std::string &d : p.activeDays) { int n = date_to_days(d); if (n < now && n > last) last = n; }
    for (const AnswerRecord &a : p.answers) { int n = date_to_days(a.date); if (n < now && n > last) last = n; }
    if (last < 0) return 0;

    std::vector<std::string> missed;
    for (int d = last + 1; d < now; d++) {
        std::string date = days_to_date(d);
        if (!p.frozenDays.count(date)) missed.push_back(date);
    }
    if (missed.empty() || (int)missed.size() > p.freezes) return 0;
    for (const std::string &d : missed) p.frozenDays.insert(d);
    p.freezes -= (int)missed.size();
    return (int)missed.size();
}

bool streak_buy_freeze(Progress &p, std::string &why)
{
    if (p.freezes >= FREEZE_MAX) { why = "You can hold " + std::to_string(FREEZE_MAX) + " at most"; return false; }
    if (p.bytes < FREEZE_PRICE) { why = "Need " + std::to_string(FREEZE_PRICE - p.bytes) + " more bytes"; return false; }
    p.bytes -= FREEZE_PRICE;
    p.freezes++;
    return true;
}

std::vector<int> streak_week(const Progress &p, const std::string &today)
{
    std::vector<int> week;
    for (int i = 6; i >= 0; i--) {
        std::string d = date_add_days(today, -i);
        week.push_back(progress_day_active(p, d) ? 1 : p.frozenDays.count(d) ? 2 : 0);
    }
    return week;
}
