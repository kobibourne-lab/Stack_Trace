// progress.cpp - loading, saving and updating progress.txt.
//
// The file looks like this (same "KEY: value" style as the content files):
//   POINTS: pointers | 60
//   ANSWER: 2026-11-03 | pointers | 1
//   SOLVED: lab1/q01_leap_year | 2026-11-03 | 154
// For ANSWER, 1 means right and 0 means wrong. For SOLVED, the last number is
// the fastest time in seconds.

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

int progress_streak(const Progress &p, const std::string &today)
{
    std::map<std::string, int> counts = answers_per_day(p);
    std::string day = today;
    if (counts.find(day) == counts.end()) day = date_add_days(today, -1);
    int streak = 0;
    while (counts.find(day) != counts.end()) {
        streak++;
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

int progress_correct_count(const Progress &p)
{
    int n = 0;
    for (const AnswerRecord &a : p.answers) if (a.correct) n++;
    return n;
}
