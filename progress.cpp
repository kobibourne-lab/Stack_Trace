// progress.cpp - loading, saving and updating progress.txt.
//
// The file looks like this (same "KEY: value" style as the content files):
//   POINTS: pointers | 60
//   ANSWER: 2026-11-03 | pointers | 1
// For ANSWER, 1 means right and 0 means wrong.

#include "progress.h"
#include "content.h"   // trim, split_fields
#include <ctime>
#include <fstream>
#include <cstdlib>

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
