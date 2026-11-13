// progress.cpp - loading, saving and updating progress.txt.
//
// The file looks like this (same "KEY: value" style as the content files):
//   POINTS: pointers | 60
//   ANSWER: 2026-11-03 | pointers | 1
//   SOLVED: lab1/q01_leap_year | 2026-11-03 | 154
//   BOSS: stack | 2026-11-10 | 401 | 2
//   BYTES: 160   (pocket money: Versus predictions, rewards, the wardrobe shop)
//   OWNED: beanie | duck   (wardrobe items bought with bytes)
//   DAYS: 2026-11-12 | 2026-11-13   (days something was learned: the streak)
//   FROZEN: 2026-11-10   (missed days a const freeze covered)
//   FREEZES: 1   (const freezes held)
//   DAILY: 2026-11-13 | 1 | correct=4 | game=2   (today's quests: the date,
//          the chests opened as bits, then the counters)
// For ANSWER, 1 means right and 0 means wrong. For SOLVED, the last number is
// the fastest time in seconds. For BOSS (a campaign boss beaten), the numbers
// are the fastest win in seconds and the most stars.

#include "progress.h"
#include "content.h"   // trim, split_fields
#include <ctime>
#include <fstream>
#include <cstdlib>
#include <cstdio>

bool progress_load(const std::string &path, Progress &p, std::string &error)
{
    p = Progress();
    std::ifstream f(path);
    if (!f) return true;   // no file yet: a fresh start, not an error
    (void)error;

    std::string raw;
    while (std::getline(f, raw)) {
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = trim(line.substr(0, colon));
        std::vector<std::string> fields = split_fields(line.substr(colon + 1), '|');

        if (key == "POINTS" && fields.size() >= 2 && !fields[0].empty()) {
            int n = std::atoi(fields[1].c_str());
            if (n < 0) n = 0;
            if (n > POINTS_MAX) n = POINTS_MAX;
            p.points[fields[0]] = n;
        } else if (key == "ANSWER" && fields.size() >= 3) {
            AnswerRecord a;
            a.date = fields[0];
            a.topic = fields[1];
            a.correct = (fields[2] == "1");
            p.answers.push_back(a);
        } else if (key == "SOLVED" && fields.size() >= 2 && !fields[0].empty()) {
            SolvedRecord s;
            s.date = fields[1];
            s.seconds = fields.size() >= 3 ? std::atoi(fields[2].c_str()) : 0;
            p.solved[fields[0]] = s;
        } else if (key == "OWNED") {
            for (const std::string &id : fields) if (!id.empty()) p.owned.insert(id);
        } else if (key == "DAYS") {
            for (const std::string &d : fields) if (date_to_days(d) >= 0) p.activeDays.insert(d);
        } else if (key == "FROZEN") {
            for (const std::string &d : fields) if (date_to_days(d) >= 0) p.frozenDays.insert(d);
        } else if (key == "FREEZES" && !fields.empty()) {
            p.freezes = std::atoi(fields[0].c_str());
            if (p.freezes < 0) p.freezes = 0;
        } else if (key == "DAILY" && fields.size() >= 2) {
            p.daily.date = fields[0];
            p.daily.claimed = std::atoi(fields[1].c_str());
            for (size_t i = 2; i < fields.size(); i++) {
                size_t eq = fields[i].find('=');
                if (eq == std::string::npos || eq == 0) continue;
                p.daily.counts[fields[i].substr(0, eq)] = std::atoi(fields[i].substr(eq + 1).c_str());
            }
        } else if (key == "BYTES" && !fields.empty()) {
            p.bytes = std::atoi(fields[0].c_str());
            if (p.bytes < 0) p.bytes = 0;
        } else if (key == "BOSS" && fields.size() >= 2 && !fields[0].empty()) {
            BossRecord b;
            b.date = fields[1];
            b.seconds = fields.size() >= 3 ? std::atoi(fields[2].c_str()) : 0;
            b.stars = fields.size() >= 4 ? std::atoi(fields[3].c_str()) : 1;
            if (b.stars < 1) b.stars = 1;
            if (b.stars > 3) b.stars = 3;
            p.bosses[fields[0]] = b;
        } else if (key == "SCORE" && fields.size() >= 2 && !fields[0].empty()) {
            p.scores[fields[0]] = std::atoi(fields[1].c_str());
        } else if (key == "WEAR" && fields.size() >= 2) {
            if (!fields[0].empty()) p.hat = fields[0];
            if (!fields[1].empty()) p.skin = fields[1];
            if (fields.size() >= 3 && !fields[2].empty()) p.pet = fields[2];   // older saves have no pet
        }
    }
    return true;
}

bool progress_save(const std::string &path, const Progress &p, std::string &error)
{
    std::ofstream f(path);
    if (!f) {
        error = "could not write " + path;
        return false;
    }
    f << "# Stack Trace progress. Safe to delete: you'll start again from nothing.\n";
    for (const auto &entry : p.points)
        f << "POINTS: " << entry.first << " | " << entry.second << "\n";
    for (const AnswerRecord &a : p.answers)
        f << "ANSWER: " << a.date << " | " << a.topic << " | " << (a.correct ? 1 : 0) << "\n";
    for (const auto &entry : p.solved)
        f << "SOLVED: " << entry.first << " | " << entry.second.date << " | " << entry.second.seconds << "\n";
    f << "BYTES: " << p.bytes << "\n";
    if (!p.owned.empty()) {
        f << "OWNED: ";
        bool first = true;
        for (const std::string &id : p.owned) { f << (first ? "" : " | ") << id; first = false; }
        f << "\n";
    }
    auto save_set = [&f](const char *key, const std::set<std::string> &items) {
        if (items.empty()) return;
        f << key << ": ";
        bool first = true;
        for (const std::string &d : items) { f << (first ? "" : " | ") << d; first = false; }
        f << "\n";
    };
    save_set("DAYS", p.activeDays);
    save_set("FROZEN", p.frozenDays);
    if (p.freezes > 0) f << "FREEZES: " << p.freezes << "\n";
    if (!p.daily.date.empty()) {
        f << "DAILY: " << p.daily.date << " | " << p.daily.claimed;
        for (const auto &c : p.daily.counts) f << " | " << c.first << "=" << c.second;
        f << "\n";
    }
    for (const auto &entry : p.bosses)
        f << "BOSS: " << entry.first << " | " << entry.second.date << " | " << entry.second.seconds << " | "
          << entry.second.stars << "\n";
    for (const auto &entry : p.scores)
        f << "SCORE: " << entry.first << " | " << entry.second << "\n";
    if (p.hat != "none" || p.skin != "classic" || p.pet != "nopet")
        f << "WEAR: " << p.hat << " | " << p.skin << " | " << p.pet << "\n";
    if (!f) {
        error = "error while writing " + path;
        return false;
    }
    return true;
}

void progress_record(Progress &p, const std::string &topic, bool correct, const std::string &date)
{
    AnswerRecord a;
    a.date = date;
    a.topic = topic;
    a.correct = correct;
    p.answers.push_back(a);

    int n = topic_points(p, topic) + (correct ? POINTS_CORRECT : -POINTS_WRONG);
    if (n < 0) n = 0;
    if (n > POINTS_MAX) n = POINTS_MAX;
    p.points[topic] = n;
}

bool progress_mark_solved(Progress &p, const std::string &problemId, const std::string &date, int seconds)
{
    auto it = p.solved.find(problemId);
    if (it == p.solved.end()) {
        SolvedRecord s;
        s.date = date;
        s.seconds = seconds;
        p.solved[problemId] = s;
        return true;
    }
    if (seconds > 0 && (it->second.seconds <= 0 || seconds < it->second.seconds)) it->second.seconds = seconds;
    return false;
}

bool progress_is_solved(const Progress &p, const std::string &problemId)
{
    return p.solved.find(problemId) != p.solved.end();
}

bool progress_record_boss(Progress &p, const std::string &regionId, const std::string &date, int seconds, int stars)
{
    if (stars < 1) stars = 1;
    if (stars > 3) stars = 3;
    auto it = p.bosses.find(regionId);
    if (it == p.bosses.end()) {
        BossRecord b;
        b.date = date;
        b.seconds = seconds;
        b.stars = stars;
        p.bosses[regionId] = b;
        return true;
    }
    if (seconds > 0 && (it->second.seconds <= 0 || seconds < it->second.seconds)) it->second.seconds = seconds;
    if (stars > it->second.stars) it->second.stars = stars;
    return false;
}

bool progress_boss_beaten(const Progress &p, const std::string &regionId)
{
    return p.bosses.find(regionId) != p.bosses.end();
}

int progress_boss_stars(const Progress &p, const std::string &regionId)
{
    auto it = p.bosses.find(regionId);
    return it == p.bosses.end() ? 0 : it->second.stars;
}

int topic_points(const Progress &p, const std::string &topic)
{
    auto it = p.points.find(topic);
    return it == p.points.end() ? 0 : it->second;
}

Mastery topic_mastery(const Progress &p, const std::string &topic)
{
    int n = topic_points(p, topic);
    if (n >= 100) return Mastery::Mastered;
    if (n >= 80) return Mastery::Proficient;
    if (n >= 50) return Mastery::Familiar;
    for (const AnswerRecord &a : p.answers)
        if (a.topic == topic) return Mastery::Attempted;
    return Mastery::NotStarted;
}

const char *mastery_name(Mastery m)
{
    switch (m) {
        case Mastery::Mastered:   return "Mastered";
        case Mastery::Proficient: return "Proficient";
        case Mastery::Familiar:   return "Familiar";
        case Mastery::Attempted:  return "Attempted";
        default:                  return "Not started";
    }
}

std::string today_string()
{
    std::time_t now = std::time(nullptr);
    std::tm *t = std::localtime(&now);
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", t);
    return buf;
}

// ---------------------------------------------------------------------------
// Dates and activity
// ---------------------------------------------------------------------------

// Days from the civil calendar date, without time zones getting involved
// (Howard Hinnant's days_from_civil algorithm).
static int days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int date_to_days(const std::string &date)
{
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return -1;
    for (int i : { 0, 1, 2, 3, 5, 6, 8, 9 })
        if (date[i] < '0' || date[i] > '9') return -1;
    int y = std::atoi(date.substr(0, 4).c_str());
    int m = std::atoi(date.substr(5, 2).c_str());
    int d = std::atoi(date.substr(8, 2).c_str());
    if (m < 1 || m > 12 || d < 1 || d > 31) return -1;
    return days_from_civil(y, m, d);
}

std::string days_to_date(int z)
{
    z += 719468;
    int era = (z >= 0 ? z : z - 146096) / 146097;
    int doe = z - era * 146097;
    int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = yoe + era * 400;
    int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int mp = (5 * doy + 2) / 153;
    int d = doy - (153 * mp + 2) / 5 + 1;
    int m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", y, m, d);
    return buf;
}

std::string date_add_days(const std::string &date, int n)
{
    int days = date_to_days(date);
    return days < 0 ? date : days_to_date(days + n);
}

int date_weekday(const std::string &date)
{
    int days = date_to_days(date);
    if (days < 0) return 0;
    return ((days % 7) + 7 + 3) % 7;   // 1970-01-01 was a Thursday
}

std::map<std::string, int> answers_per_day(const Progress &p)
{
    std::map<std::string, int> counts;
    for (const AnswerRecord &a : p.answers) counts[a.date]++;
    return counts;
}

bool progress_day_active(const Progress &p, const std::string &date)
{
    if (p.activeDays.count(date)) return true;
    for (const AnswerRecord &a : p.answers) if (a.date == date) return true;
    return false;
}

int progress_streak(const Progress &p, const std::string &today)
{
    std::set<std::string> active = p.activeDays;
    for (const AnswerRecord &a : p.answers) active.insert(a.date);
    std::string day = today;
    if (!active.count(day)) day = date_add_days(today, -1);
    int streak = 0;
    for (;;) {
        if (active.count(day)) streak++;
        else if (!p.frozenDays.count(day)) break;   // a frozen day keeps it going without adding to it
        day = date_add_days(day, -1);
    }
    return streak;
}

int progress_total_points(const Progress &p)
{
    int total = 0;
    for (const auto &entry : p.points) total += entry.second;
    return total;
}

int quiz_mastery_points(int livesLeft, int maxLives)
{
    if (livesLeft <= 0) return 0;
    int points = POINTS_MAX - 20 * (maxLives - livesLeft);
    return points < 0 ? 0 : points;
}

bool progress_finish_quiz(Progress &p, const std::string &topic, int livesLeft, int maxLives)
{
    if (livesLeft <= 0) return false;
    int earned = quiz_mastery_points(livesLeft, maxLives);
    if (earned > p.points[topic]) p.points[topic] = earned;
    return true;
}

bool progress_record_score(Progress &p, const std::string &game, int score)
{
    auto it = p.scores.find(game);
    if (it != p.scores.end() && it->second >= score) return false;
    p.scores[game] = score;
    return true;
}

int progress_best_score(const Progress &p, const std::string &game)
{
    auto it = p.scores.find(game);
    return it == p.scores.end() ? 0 : it->second;
}

int progress_xp(const Progress &p)
{
    int xp = 0;
    for (const AnswerRecord &a : p.answers) xp += a.correct ? XP_CORRECT : XP_WRONG;
    xp += (int)p.solved.size() * XP_SOLVED;
    for (const auto &entry : p.bosses) xp += XP_BOSS + entry.second.stars * XP_STAR;
    return xp;
}

int xp_for_level(int level)
{
    if (level <= 1) return 0;
    return 25 * level * (level - 1);   // 50, 150, 300, 500, 750 ...
}

int level_for_xp(int xp)
{
    int level = 1;
    while (xp >= xp_for_level(level + 1)) level++;
    return level;
}

std::string level_title(int level)
{
    static const char *titles[] = {
        "Hello, World",        // 1
        "printf Apprentice",   // 2
        "Loop Runner",         // 3
        "Array Walker",        // 4
        "Pointer Apprentice",  // 5
        "Address Hunter",      // 6
        "Heap Explorer",       // 7
        "Leak Hunter",         // 8
        "List Linker",         // 9
        "Class Architect",     // 10
        "Segfault Survivor",   // 11
        "Memory Wizard",       // 12 and up
    };
    const int count = (int)(sizeof(titles) / sizeof(titles[0]));
    if (level < 1) level = 1;
    return titles[level > count ? count - 1 : level - 1];
}

int progress_correct_count(const Progress &p)
{
    int n = 0;
    for (const AnswerRecord &a : p.answers) if (a.correct) n++;
    return n;
}
