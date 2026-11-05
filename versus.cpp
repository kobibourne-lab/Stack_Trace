// versus.cpp - the rules of a Versus match. See versus.h.

#include "versus.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

VsMessage vs_parse(const std::string &line)
{
    VsMessage msg;
    size_t space = line.find(' ');
    msg.kind = line.substr(0, space);
    if (space != std::string::npos) msg.rest = line.substr(space + 1);
    std::stringstream ss(msg.rest);
    std::string word;
    while (ss >> word) msg.args.push_back(word);
    return msg;
}

std::string vs_escape(const std::string &code)
{
    std::string out;
    for (char c : code) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if (c != '\r') out += c;
    }
    return out;
}

std::string vs_unescape(const std::string &text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char n = text[++i];
            if (n == 'n') out += '\n';
            else if (n == 't') out += '\t';
            else out += n;
        } else {
            out += text[i];
        }
    }
    return out;
}

std::string vs_result_word(const RunResult &r)
{
    switch (r.status) {
        case RunStatus::Passed:       return r.ruleProblems.empty() ? "passed" : "rule";
        case RunStatus::Failed:       return "wrong";
        case RunStatus::CompileError: return "compile";
        case RunStatus::Crashed:      return "crash";
        case RunStatus::TimedOut:     return "timeout";
        default:                      return "compile";
    }
}

std::string vs_result_text(const std::string &word, int passed, int total)
{
    std::string tests = std::to_string(passed) + " / " + std::to_string(total) + " tests";
    if (word == "compile") return "compile error";
    if (word == "crash") return "crashed after " + tests;
    if (word == "timeout") return "time limit, " + tests;
    if (word == "rule") return tests + ", but broke a rule";
    return tests;
}

static int to_int(const std::string &s) { return std::atoi(s.c_str()); }

void vs_begin(VsMatch &m, const std::string &setId, const std::string &problemId, int minutes, double now)
{
    std::string myName = m.me.name, theirName = m.them.name;
    m.me = VsPlayer();
    m.them = VsPlayer();
    m.me.name = myName;
    m.them.name = theirName;
    m.feed.clear();
    m.setId = setId;
    m.problemId = problemId;
    m.minutes = minutes > 0 ? minutes : 15;
    m.phase = VsPhase::Countdown;
    m.startAt = now + COUNTDOWN_SECONDS;
    m.endedAt = 0;
    m.scored = false;
    m.notice.clear();
}

double vs_elapsed(const VsMatch &m, double now)
{
    double t = (m.endedAt > 0 ? m.endedAt : now) - m.startAt;
    return t < 0 ? 0 : t;
}

double vs_remaining(const VsMatch &m, double now)
{
    double left = m.minutes * 60.0 - vs_elapsed(m, now);
    return left < 0 ? 0 : left;
}

static void add_event(VsMatch &m, double now, const std::string &text, int kind)
{
    m.feed.push_back({ vs_elapsed(m, now), text, kind });
}

void vs_apply(VsMatch &m, const std::string &line, double now)
{
    VsMessage msg = vs_parse(line);
    VsPlayer &them = m.them;

    if (msg.kind == "HELLO") {
        if (msg.args.empty() || to_int(msg.args[0]) != VERSUS_VERSION) {
            m.notice = "You're on different versions of Stack Trace. Both pull the latest main and rebuild.";
            return;
        }
        size_t space = msg.rest.find(' ');
        them.name = (space == std::string::npos) ? "Opponent" : msg.rest.substr(space + 1);
        if (them.name.empty()) them.name = "Opponent";
        m.helloReceived = true;
    } else if (msg.kind == "SETTINGS" && msg.args.size() >= 3 && !m.host) {
        m.lobbySet = msg.args[0];
        m.lobbyDifficulty = msg.args[1];
        m.lobbyMinutes = to_int(msg.args[2]);
    } else if (msg.kind == "START" && msg.args.size() >= 3 && !m.host) {
        vs_begin(m, msg.args[0], msg.args[1], to_int(msg.args[2]), now);
        m.startRequested = true;
    } else if (msg.kind == "MISSING" && m.host) {
        m.phase = VsPhase::Lobby;
        m.notice = them.name + " doesn't have that problem. Both pull the latest main and rebuild.";
    } else if (msg.kind == "RUNNING") {
        them.running = true;
    } else if (msg.kind == "RAN" && msg.args.size() >= 3) {
        them.running = false;
        them.runs++;
        them.passed = to_int(msg.args[0]);
        them.total = to_int(msg.args[1]);
        them.lastResult = msg.args[2];
        if (them.lastResult == "compile") them.compileErrors++;
        if (them.lastResult != "compile" && them.lastResult != "rule" && them.passed > them.bestPassed) them.bestPassed = them.passed;
        int kind = (them.lastResult == "compile" || them.passed == 0) ? 1 : 2;
        if (m.phase == VsPhase::Playing) add_event(m, now, "Ran: " + vs_result_text(them.lastResult, them.passed, them.total), kind);
    } else if (msg.kind == "DONE" && !msg.args.empty()) {
        them.finished = true;
        them.over = true;
        them.finishSeconds = std::atof(msg.args[0].c_str());
        them.bestPassed = them.total > 0 ? them.total : them.bestPassed;
        add_event(m, now, "Accepted!", 3);
    } else if (msg.kind == "OVER") {
        them.over = true;
    } else if (msg.kind == "LINES" && !msg.args.empty()) {
        them.lines = to_int(msg.args[0]);
    } else if (msg.kind == "CODE") {
        them.code = vs_unescape(msg.rest);
        them.hasCode = true;
    } else if (msg.kind == "LEFT") {
        them.left = true;
        them.over = true;
        if (m.phase == VsPhase::Playing || m.phase == VsPhase::Countdown) add_event(m, now, "Left the match", 1);
    } else if (msg.kind == "REMATCH") {
        m.phase = VsPhase::Lobby;
        m.notice = them.name + " wants a rematch.";
    }
    // Anything else is ignored, so a newer version's extra messages don't break this one
}

std::vector<std::string> vs_my_run(VsMatch &m, const RunResult &r, bool submitted, double now)
{
    std::vector<std::string> out;
    VsPlayer &me = m.me;
    std::string word = vs_result_word(r);
    me.running = false;
    me.runs++;
    me.passed = r.passed;
    me.total = r.passed + r.failed;
    me.lastResult = word;
    if (word == "compile") me.compileErrors++;
    if (word != "compile" && word != "rule" && me.passed > me.bestPassed) me.bestPassed = me.passed;
    out.push_back("RAN " + std::to_string(me.passed) + " " + std::to_string(me.total) + " " + word);

    if (submitted && run_accepted(r) && !me.finished) {
        me.finished = true;
        me.over = true;
        double t = vs_elapsed(m, now);
        if (t > m.minutes * 60.0) t = m.minutes * 60.0;
        me.finishSeconds = t;
        char buf[32];
        std::snprintf(buf, sizeof buf, "DONE %.1f", t);
        out.push_back(buf);
    }
    return out;
}

std::string vs_my_time_up(VsMatch &m)
{
    m.me.over = true;
    return "OVER";
}

bool vs_check_end(VsMatch &m, double now)
{
    if (m.phase == VsPhase::Countdown && now >= m.startAt) m.phase = VsPhase::Playing;
    if (m.phase != VsPhase::Playing) return m.phase == VsPhase::Finished;

    double limit = m.minutes * 60.0;
    double t = now - m.startAt;
    bool ended = (m.me.over && m.them.over) || m.me.left || m.them.left ||
                 (t >= limit && m.me.over && t >= limit + END_GRACE_SECONDS);
    if (!ended) return false;

    m.phase = VsPhase::Finished;
    m.endedAt = now;
    if (!m.scored) {
        std::string why;
        int r = vs_outcome(m, why);
        if (r > 0) m.myWins++;
        else if (r < 0) m.theirWins++;
        else m.draws++;
        m.scored = true;
    }
    return true;
}

int vs_outcome(const VsMatch &m, std::string &reason)
{
    const VsPlayer &me = m.me, &them = m.them;
    if (me.finished && them.finished) {
        if (me.finishSeconds < them.finishSeconds) { reason = "Both solved it: you were faster."; return 1; }
        if (them.finishSeconds < me.finishSeconds) { reason = "Both solved it: " + them.name + " was faster."; return -1; }
        reason = "Both solved it in the same time.";
        return 0;
    }
    if (me.finished) { reason = them.left ? them.name + " left before finishing." : "Only you solved it."; return 1; }
    if (them.finished) { reason = me.left ? "You left before finishing." : "Only " + them.name + " solved it."; return -1; }
    if (them.left && !me.left) { reason = them.name + " left the match."; return 1; }
    if (me.left && !them.left) { reason = "You left the match."; return -1; }
    if (me.bestPassed > them.bestPassed) { reason = "Nobody solved it, but you passed more tests."; return 1; }
    if (them.bestPassed > me.bestPassed) { reason = "Nobody solved it, but " + them.name + " passed more tests."; return -1; }
    reason = "Nobody solved it, and you passed the same number of tests.";
    return 0;
}

std::string vs_default_name()
{
    const char *n = std::getenv("USERNAME");    // Windows
    if (!n || !*n) n = std::getenv("USER");     // Mac and Linux
    std::string name = (n && *n) ? n : "Player";
    std::string clean;
    for (char c : name) if (isalnum((unsigned char)c) && clean.size() < 16) clean += c;
    if (clean.empty()) clean = "Player";
    clean[0] = (char)toupper((unsigned char)clean[0]);
    return clean;
}
