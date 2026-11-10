// campaign.cpp - loading content/campaign.txt, and the campaign's rules. See campaign.h.

#include "campaign.h"
#include "content.h"   // trim, split_fields
#include <cctype>
#include <fstream>
#include <sstream>

bool load_campaign(const std::string &path, std::vector<Region> &regions, std::string &error)
{
    std::ifstream f(path);
    if (!f) {
        error = "could not open " + path;
        regions.clear();
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parse_campaign(ss.str(), regions, error);
}

bool parse_campaign(const std::string &text, std::vector<Region> &regions, std::string &error)
{
    regions.clear();
    std::stringstream in(text);
    std::string raw;
    int lineNo = 0;
    while (std::getline(in, raw)) {
        lineNo++;
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = trim(line.substr(0, colon));
        std::string value = trim(line.substr(colon + 1));
        std::vector<std::string> fields = split_fields(value, '|');

        if (key == "REGION") {
            if (fields.size() < 2 || fields[0].empty()) {
                error += "line " + std::to_string(lineNo) + ": a REGION needs at least an id and a name. ";
                continue;
            }
            Region r;
            r.id = fields[0];
            r.name = fields[1];
            if (fields.size() > 2) r.covers = fields[2];
            if (fields.size() > 3 && fields[3] != "-") r.exam = fields[3];
            regions.push_back(r);
            continue;
        }
        if (regions.empty()) continue;   // anything before the first REGION is ignored
        Region &r = regions.back();
        if (key == "TOPICS") {
            for (const std::string &t : split_fields(value, ',')) if (!t.empty()) r.topics.push_back(t);
        } else if (key == "BOSS" && fields.size() >= 2) {
            r.bossName = fields[0];
            r.bossProblem = fields[1];
            if (fields.size() > 2) r.bossLook = fields[2];
        } else if (key == "TAUNT") {
            r.taunt = value;
        } else if (key == "STORY") {
            r.story = value;
        } else if (key == "WIN") {
            r.win = value;
        }
    }

    // A region with no topics or no boss can't be played: leave it out and say so
    std::vector<Region> ok;
    for (const Region &r : regions) {
        if (r.topics.empty()) error += r.id + " has no TOPICS. ";
        else if (r.bossProblem.empty()) error += r.id + " has no BOSS. ";
        else ok.push_back(r);
    }
    regions = ok;
    if (regions.empty()) {
        if (error.empty()) error = "no regions found";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------

bool topic_cleared(const Progress &p, const std::string &topic)
{
    Mastery m = topic_mastery(p, topic);
    return m == Mastery::Familiar || m == Mastery::Proficient || m == Mastery::Mastered;
}

bool region_beaten(const std::vector<Region> &regions, const Progress &p, int region)
{
    if (region < 0 || region >= (int)regions.size()) return false;
    return progress_boss_beaten(p, regions[region].id);
}

bool region_open(const std::vector<Region> &regions, const Progress &p, int region)
{
    if (region < 0 || region >= (int)regions.size()) return false;
    return region == 0 || region_beaten(regions, p, region - 1);
}

int region_topics_left(const Region &r, const Progress &p)
{
    int left = 0;
    for (const std::string &t : r.topics) if (!topic_cleared(p, t)) left++;
    return left;
}

bool boss_unlocked(const std::vector<Region> &regions, const Progress &p, int region)
{
    if (!region_open(regions, p, region)) return false;
    return region_beaten(regions, p, region) || region_topics_left(regions[region], p) == 0;
}

int current_region(const std::vector<Region> &regions, const Progress &p)
{
    for (int i = 0; i < (int)regions.size(); i++)
        if (region_open(regions, p, i) && !region_beaten(regions, p, i)) return i;
    return regions.empty() ? 0 : (int)regions.size() - 1;
}

int next_topic(const Region &r, const Progress &p)
{
    for (int i = 0; i < (int)r.topics.size(); i++)
        if (!topic_cleared(p, r.topics[i])) return i;
    return -1;
}

// ---------------------------------------------------------------------------

int count_tests(const Problem &p)
{
    // Every check is a call like checkInt( or checkArray( or checkOutput(
    int n = 0;
    for (const std::string &line : p.tests) {
        size_t at = 0;
        while ((at = line.find("check", at)) != std::string::npos) {
            bool startOfWord = at == 0 || !(isalnum((unsigned char)line[at - 1]) || line[at - 1] == '_');
            size_t end = at + 5;
            bool capital = end < line.size() && line[end] >= 'A' && line[end] <= 'Z';
            while (end < line.size() && (isalnum((unsigned char)line[end]) || line[end] == '_')) end++;
            while (end < line.size() && line[end] == ' ') end++;
            if (startOfWord && capital && end < line.size() && (line[end] == '(' || line[end] == '<')) n++;
            at += 5;
        }
    }
    return n;
}

int boss_hp(int totalTests, int bestPassed)
{
    int hp = totalTests - bestPassed;
    return hp < 0 ? 0 : hp;
}

int boss_stars(bool won, int livesLost, double seconds, double limitSeconds)
{
    if (!won) return 0;
    int stars = 1;
    if (livesLost == 0) stars++;
    if (limitSeconds > 0 && seconds <= limitSeconds / 2) stars++;
    return stars;
}
