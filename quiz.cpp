// quiz.cpp - score and lives for one run through a quiz.

#include "quiz.h"

void quiz_start(QuizSession &s, const Quiz &quiz)
{
    s = QuizSession();
    s.quiz = quiz;
}

// The last answer's feedback stays on screen even if it used the last life,
// so the quiz only counts as over once the player has moved on from it.
bool quiz_finished(const QuizSession &s)
{
    if (s.index >= (int)s.quiz.questions.size()) return true;
    return s.lives <= 0 && !s.answered;
}

const Question *quiz_current(const QuizSession &s)
{
    if (quiz_finished(s)) return nullptr;
    return &s.quiz.questions[s.index];
}

bool quiz_answer(QuizSession &s, int choice)
{
    const Question *q = quiz_current(s);
    if (q == nullptr || s.answered || s.lives <= 0) return false;

    s.answered = true;
    s.chosen = choice;
    s.lastCorrect = (choice == q->answer);
    if (s.lastCorrect) {
        s.score++;
    } else {
        s.lives--;
        s.missed.push_back(s.index);
    }
    return s.lastCorrect;
}

bool quiz_is_last(const QuizSession &s)
{
    if (!s.answered) return false;
    return s.lives <= 0 || s.index + 1 >= (int)s.quiz.questions.size();
}

void quiz_next(QuizSession &s)
{
    if (!s.answered) return;   // must answer first
    s.index++;
    s.answered = false;
    s.chosen = 0;
}
