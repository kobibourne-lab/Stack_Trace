// examq.cpp - the Exam question of the day (see examq.h).

#include "examq.h"
#include <random>

bool examq_load(const std::vector<Topic> &topics, const std::string &contentDir, std::vector<ExamQ> &pool, std::string &error)
{
    pool.clear();
    for (const Topic &t : topics) {
        if (t.quizFile.empty()) continue;
        Quiz quiz;
        std::string why;
        if (!load_quiz(contentDir + "/" + t.quizFile, quiz, why)) continue;   // skipped, the rest still work
        for (size_t i = 0; i < quiz.questions.size(); i++) {
            ExamQ e;
            e.topic = t.id;
            e.topicTitle = t.title;
            e.exam = t.exam;
            e.number = (int)i + 1;
            e.q = quiz.questions[i];
            pool.push_back(e);
        }
    }
    if (pool.empty()) { error = "No quiz questions found in " + contentDir + "/quizzes"; return false; }
    return true;
}

// The pool in a shuffled order that belongs to one cycle (cycle 0, 1, 2...).
// mt19937 gives the same numbers on every computer, so everyone gets the same day.
static std::vector<int> cycle_order(int poolSize, int cycle)
{
    std::vector<int> order(poolSize);
    for (int i = 0; i < poolSize; i++) order[i] = i;
    std::mt19937 rng(2654435761u * (unsigned int)(cycle + 1) + 12345u);
    for (int i = poolSize - 1; i > 0; i--) std::swap(order[i], order[rng() % (unsigned int)(i + 1)]);
    return order;
}

int examq_index(int poolSize, const std::string &date)
{
    int day = date_to_days(date);
    if (poolSize <= 0 || day < 0) return 0;
    int cycle = day / poolSize, at = day % poolSize;
    std::vector<int> order = cycle_order(poolSize, cycle);
    // The first question of a cycle shouldn't be the last of the one before
    if (cycle > 0 && poolSize > 2 && order[0] == cycle_order(poolSize, cycle - 1)[poolSize - 1]) std::swap(order[0], order[1]);
    return order[at];
}

int examq_number(const std::string &date)
{
    int n = date_to_days(date) - date_to_days(EXAMQ_START) + 1;
    return n < 1 ? 1 : n;
}

bool examq_answered(const Progress &p, const std::string &date)
{
    return p.examq.date == date && p.examq.choice > 0;
}

bool examq_correct(const Progress &p, const std::string &date)
{
    return examq_answered(p, date) && p.examq.correct;
}

int examq_choice(const Progress &p, const std::string &date)
{
    return examq_answered(p, date) ? p.examq.choice : 0;
}

int examq_streak(const Progress &p, const std::string &today)
{
    const ExamQStats &s = p.examqStats;
    if (s.streak <= 0 || s.lastCorrect.empty()) return 0;
    if (s.lastCorrect == today || s.lastCorrect == date_add_days(today, -1)) return s.streak;
    return 0;
}

int examq_reward(bool correct, int streakAfter)
{
    if (!correct) return EXAMQ_TRY_BYTES;
    int extra = streakAfter - 1;
    if (extra < 0) extra = 0;
    if (extra > EXAMQ_STREAK_MAX) extra = EXAMQ_STREAK_MAX;
    return EXAMQ_BYTES + extra * EXAMQ_STREAK_BONUS;
}

bool examq_answer(Progress &p, const std::string &date, const ExamQ &q, int choice, bool &correct, int &bytesWon, std::string &why)
{
    correct = false;
    bytesWon = 0;
    if (date_to_days(date) < 0) { why = "Not a date"; return false; }
    if (examq_answered(p, date)) { why = "You've already answered today's question"; return false; }
    int most = q.q.kind == QuestionKind::Line ? (int)q.q.code.size() : (int)q.q.options.size();
    if (choice < 1 || choice > most) { why = "Pick one of the answers"; return false; }

    correct = choice == q.q.answer;
    p.examq.date = date;
    p.examq.choice = choice;
    p.examq.correct = correct;

    ExamQStats &s = p.examqStats;
    s.played++;
    if (correct) {
        s.correct++;
        s.streak = (s.lastCorrect == date_add_days(date, -1)) ? s.streak + 1 : 1;
        s.lastCorrect = date;
        if (s.streak > s.best) s.best = s.streak;
    } else {
        s.streak = 0;
    }
    bytesWon = examq_reward(correct, s.streak);
    p.bytes += bytesWon;
    return true;
}

std::string examq_share(const Progress &p, const std::string &date)
{
    std::string out = "Exam Q #" + std::to_string(examq_number(date)) + ": ";
    if (!examq_answered(p, date)) return out + "not answered yet";
    out += examq_correct(p, date) ? "right" : "wrong";
    int streak = examq_streak(p, date);
    if (streak > 1) out += " (" + std::to_string(streak) + " day streak)";
    return out;
}
