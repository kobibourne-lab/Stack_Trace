// screen_placeholder.cpp - Exam prep and Profile, until they're built.
// Each says what it will show (from the mockup) so whoever picks it up
// knows where to start. Replace this file's case with a real screen.

#include "app.h"
#include "theme.h"
#include "ui.h"

void screen_placeholder(App &app)
{
    if (IsKeyPressed(KEY_ESCAPE)) { open_map(app); return; }

    bool exam = (app.screen == Screen::ExamPrep);
    ui_header(exam ? "Exam prep" : "Profile");

    float x = MARGIN, y = HEADER_H + 40, w = 760;
    ui_text(exam ? "Exam prep" : "Profile", x, y, 32, COL_TEXT, FontKind::UiBold);
    y += 52;
    ui_text("Not built yet. Owner: ______", x, y, 16, COL_AMBER, FontKind::Mono);
    y += 40;

    std::string plan = exam
        ? "NeetCode style. The 10 functions from the Coding Exam 2 Preparation lab (swap_ints, swap_chars, "
          "find_max, reverse_array, strlen_ptr, count_vowels, copy_string, concat_strings, sum_array, "
          "compare_strings), grouped by topic with Easy / Medium / Hard tags and a solved tick.\n"
          "Each opens a problem card: the prototype, an example, and an exam mode with a timer.\n"
          "The lab's rule (no arr[i], pointer arithmetic only) could be checked by looking for [ in the answer."
        : "GitHub style. Avatar and name, current streak and mastery points, achievement badges, "
          "pinned topics with their mastery level, and a heatmap of questions answered per day.\n"
          "Everything it needs is already in progress.txt: the points per topic, and every answer with its date.";
    ui_text_wrapped(plan, x, y, w, 17, COL_TEXT_SOFT);

    if (ui_button("Back to the map", { (float)MARGIN, SCREEN_H - FOOTER_H - 70.0f, 180, 46 }, ButtonStyle::Outline, COL_TEXT))
        open_map(app);

    ui_footer_hints({ "ESC topic map" });
}
