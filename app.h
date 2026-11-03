// app.h - everything the screens share, and the functions that move between them.

#pragma once
#include <string>
#include <vector>
#include "content.h"
#include "quiz.h"
#include "progress.h"

enum class Screen { Map, Lesson, Quiz, ExamPrep, Profile };

struct App {
    Screen screen = Screen::Map;
    bool quit = false;

    std::vector<Topic> topics;
    int selected = 0;            // the topic picked on the map

    Progress progress;
    std::string progressPath = "progress.txt";

    Lesson lesson;               // the open lesson
    int step = 0;                // which of its steps is showing (0 = first)

    QuizSession quiz;            // the quiz in progress

    int examSelected = 0;        // the problem picked on the Exam prep screen
    std::string examMessage;     // shown on its card, e.g. "not built yet"

    std::string message;         // shown on the map, e.g. a file that wouldn't load
};

// Moving between screens. open_lesson and open_quiz load the files for the
// selected topic; if that fails they stay on the map and set app.message.
void open_map(App &app);
void open_lesson(App &app);
void open_quiz(App &app);

// Records an answer for the topic and saves progress.txt straight away.
void record_answer(App &app, const std::string &topic, bool correct);

// One function per screen: handles its input and draws it, once per frame.
void screen_map(App &app);
void screen_lesson(App &app);
void screen_quiz(App &app);
void screen_exam(App &app);
void screen_profile(App &app);
