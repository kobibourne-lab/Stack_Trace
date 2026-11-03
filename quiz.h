// quiz.h - running a quiz: which question we're on, score, lives.
// No drawing in here; screen_quiz.cpp draws it and calls these functions.

#pragma once
#include "content.h"

const int START_LIVES = 3;

struct QuizSession {
    Quiz quiz;
    int index = 0;          // current question (0 = first)
    int score = 0;          // questions answered correctly
    int lives = START_LIVES;
    bool answered = false;  // has the current question been answered yet?
    int chosen = 0;         // what was picked (option or line number, 1 = first)
    bool lastCorrect = false;
    std::vector<int> missed;  // indexes of the questions answered wrongly
};

void quiz_start(QuizSession &s, const Quiz &quiz);

// The question on screen, or nullptr once the quiz is over.
const Question *quiz_current(const QuizSession &s);

// Answer the current question. Returns true if it was right.
// Does nothing (returns false) if it was already answered or the quiz is over.
bool quiz_answer(QuizSession &s, int choice);

// Move on after answering.
void quiz_next(QuizSession &s);

// Over when every question is done or the lives have run out.
bool quiz_finished(const QuizSession &s);

// True when the current question is answered and moving on will end the quiz
// (it was the last question, or the last life). The screen says "See results".
bool quiz_is_last(const QuizSession &s);
