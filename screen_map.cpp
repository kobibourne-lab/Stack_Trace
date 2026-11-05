// screen_map.cpp - the topic map: one row per track, a circle per topic
// coloured by mastery, and a card for the selected topic.

#include "app.h"
#include "theme.h"
#include "ui.h"

const float MAP_W = 840;       // the area the tracks are drawn in
const float SLOT_W = 120;      // space per topic, so columns line up across rows
const float NODE_R = 28;
const float ROW_H = 124;

// The tracks in the order they first appear in topics.txt.
static std::vector<std::string> track_names(const App &app)
{
    std::vector<std::string> names;
    for (const Topic &t : app.topics) {
        bool seen = false;
        for (const std::string &n : names) if (n == t.track) seen = true;
        if (!seen) names.push_back(t.track);
    }
    return names;
}

// Indexes (into app.topics) of the topics in one track.
static std::vector<int> topics_in(const App &app, const std::string &track)
{
    std::vector<int> list;
    for (int i = 0; i < (int)app.topics.size(); i++)
        if (app.topics[i].track == track) list.push_back(i);
    return list;
}

static std::string two_digits(int n)
{
    return (n < 10 ? "0" : "") + std::to_string(n);
}

// Arrow keys: left/right through every topic, up/down between tracks.
static void handle_keys(App &app)
{
    int count = (int)app.topics.size();
    if (IsKeyPressed(KEY_RIGHT)) app.selected = (app.selected + 1) % count;
    if (IsKeyPressed(KEY_LEFT)) app.selected = (app.selected + count - 1) % count;

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        std::vector<std::string> tracks = track_names(app);
        int row = 0, col = 0;
        for (int r = 0; r < (int)tracks.size(); r++) {
            std::vector<int> list = topics_in(app, tracks[r]);
            for (int c = 0; c < (int)list.size(); c++)
                if (list[c] == app.selected) { row = r; col = c; }
        }
        row += IsKeyPressed(KEY_DOWN) ? 1 : -1;
        if (row >= 0 && row < (int)tracks.size()) {
            std::vector<int> list = topics_in(app, tracks[row]);
            app.selected = list[col < (int)list.size() ? col : list.size() - 1];
        }
    }

    if (IsKeyPressed(KEY_ENTER)) open_lesson(app);
    if (IsKeyPressed(KEY_Q)) open_quiz(app);
    if (IsKeyPressed(KEY_E)) app.screen = Screen::ExamPrep;
    if (IsKeyPressed(KEY_P)) app.screen = Screen::Profile;
    if (IsKeyPressed(KEY_ESCAPE)) app.screen = Screen::Title;
}

static void draw_course_mastery(const App &app, float x, float y)
{
    int total = 0;
    for (const Topic &t : app.topics) total += topic_points(app.progress, t.id);
    int max = (int)app.topics.size() * POINTS_MAX;

    ui_text("Course mastery", x, y, 15, COL_TEXT, FontKind::UiBold);
    std::string pts = std::to_string(total) + " / " + std::to_string(max) + " points";
    ui_text(pts, x + MAP_W - ui_text_width(pts, 13, FontKind::Mono), y + 2, 13, COL_TEXT_SOFT, FontKind::Mono);

    Rectangle bar = { x, y + 26, MAP_W, 8 };
    DrawRectangleRounded(bar, 1, 6, COL_LINE_SOFT);
    if (total > 0) DrawRectangleRounded({ bar.x, bar.y, bar.width * total / max, bar.height }, 1, 6, COL_GREEN);
}

static void draw_track(App &app, const std::string &track, float x, float y, int &number)
{
    std::vector<int> list = topics_in(app, track);

    // Title, the exam tag (if the whole track shares one) and a count
    std::string upper = track;
    for (char &c : upper) c = (char)toupper((unsigned char)c);
    ui_text(upper, x, y, 14, COL_TEXT, FontKind::MonoBold);
    std::string exam = app.topics[list[0]].exam;
    for (int i : list) if (app.topics[i].exam != exam) exam.clear();
    if (!exam.empty()) {
        float tx = x + ui_text_width(upper, 14, FontKind::MonoBold) + 14;
        float w = ui_text_width(exam, 12, FontKind::Mono) + 14;
        Color tag = (exam == "Exam 2") ? COL_PURPLE : COL_BLUE;
        DrawRectangleRounded({ tx, y - 1, w, 19 }, 0.3f, 4, tag);
        ui_text(exam, tx + 7, y + 2, 12, COL_BG, FontKind::Mono);
    }
    int mastered = 0;
    for (int i : list) if (topic_mastery(app.progress, app.topics[i].id) == Mastery::Mastered) mastered++;
    std::string count = std::to_string(mastered) + " of " + std::to_string(list.size()) + " mastered";
    ui_text(count, x + MAP_W - ui_text_width(count, 13), y + 1, 13, COL_MUTED);

    // A line behind the circles, then the circles
    float cy = y + 30 + NODE_R;
    float firstX = x + SLOT_W / 2, lastX = x + SLOT_W / 2 + (list.size() - 1) * SLOT_W;
    DrawLineEx({ firstX, cy }, { lastX, cy }, 2, COL_LINE);

    for (int c = 0; c < (int)list.size(); c++) {
        int index = list[c];
        const Topic &t = app.topics[index];
        Vector2 centre = { x + SLOT_W / 2 + c * SLOT_W, cy };
        Mastery m = topic_mastery(app.progress, t.id);
        Color ring = mastery_color(m);
        bool hasContent = !t.lessonFile.empty() || !t.quizFile.empty();

        bool hover = CheckCollisionPointCircle(GetMousePosition(), centre, NODE_R + 4);
        if (hover) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { app.selected = index; app.message.clear(); }
        }

        if (index == app.selected) DrawCircleV(centre, NODE_R + 7, Fade(COL_AMBER, 0.25f));
        DrawCircleV(centre, NODE_R, index == app.selected ? COL_AMBER : ring);
        DrawCircleV(centre, NODE_R - 3, hover ? COL_SURFACE_2 : COL_BG);

        std::string num = two_digits(number++);
        Color numColor = (m == Mastery::NotStarted) ? (hasContent ? COL_TEXT_SOFT : COL_MUTED) : ring;
        float w = ui_text_width(num, 17, FontKind::MonoBold);
        ui_text(num, centre.x - w / 2, centre.y - 10, 17, numColor, FontKind::MonoBold);

        ui_text_centered_wrapped(t.title, centre.x, cy + NODE_R + 6, SLOT_W - 12, 13,
                                 hasContent ? COL_TEXT : COL_MUTED, 2);
    }
}

static void draw_topic_card(App &app, Rectangle r)
{
    const Topic &t = app.topics[app.selected];
    Mastery m = topic_mastery(app.progress, t.id);
    int points = topic_points(app.progress, t.id);

    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    float x = r.x + 24, y = r.y + 22, w = r.width - 48;

    std::string eyebrow = two_digits(app.selected + 1) + "  |  " + t.track;
    ui_text(eyebrow, x, y, 12, m == Mastery::NotStarted ? COL_MUTED : mastery_color(m), FontKind::Mono);
    y += 26;
    y += ui_text_wrapped(t.title, x, y, w, 26, COL_TEXT, FontKind::UiBold) + 6;
    if (!t.exam.empty()) {
        ui_text("Coding " + t.exam, x, y, 14, COL_TEXT_SOFT);
        y += 26;
    }

    // Mastery
    ui_text(mastery_name(m), x, y, 15, m == Mastery::NotStarted ? COL_MUTED : mastery_color(m), FontKind::UiBold);
    std::string pts = std::to_string(points) + " / " + std::to_string(POINTS_MAX) + " pts";
    ui_text(pts, x + w - ui_text_width(pts, 13, FontKind::Mono), y + 2, 13, COL_TEXT_SOFT, FontKind::Mono);
    y += 26;
    DrawRectangleRounded({ x, y, w, 6 }, 1, 6, COL_LINE_SOFT);
    if (points > 0) DrawRectangleRounded({ x, y, w * points / POINTS_MAX, 6 }, 1, 6, mastery_color(m));
    y += 24;

    // What's ready
    ui_text("Lesson", x, y, 14, COL_TEXT_SOFT);
    ui_text(t.lessonFile.empty() ? "not written yet" : "ready", x + w - ui_text_width(t.lessonFile.empty() ? "not written yet" : "ready", 13, FontKind::Mono),
            y + 1, 13, t.lessonFile.empty() ? COL_MUTED : COL_GREEN, FontKind::Mono);
    y += 24;
    ui_text("Quiz", x, y, 14, COL_TEXT_SOFT);
    ui_text(t.quizFile.empty() ? "not written yet" : "ready", x + w - ui_text_width(t.quizFile.empty() ? "not written yet" : "ready", 13, FontKind::Mono),
            y + 1, 13, t.quizFile.empty() ? COL_MUTED : COL_GREEN, FontKind::Mono);
    y += 36;

    float bw = (w - 12) / 2;
    if (ui_button("Start lesson", { x, y, bw, 46 }, t.lessonFile.empty() ? ButtonStyle::Disabled : ButtonStyle::Primary))
        open_lesson(app);
    if (ui_button("Quiz me", { x + bw + 12, y, bw, 46 }, t.quizFile.empty() ? ButtonStyle::Disabled : ButtonStyle::Outline, COL_TEXT))
        open_quiz(app);
    y += 66;

    if (!app.message.empty()) ui_text_wrapped(app.message, x, y, w, 14, COL_CORAL);
}

void screen_map(App &app)
{
    handle_keys(app);
    ui_header("Learn C & C++ by watching memory move");

    // Header buttons, top right
    if (ui_button("Profile", { SCREEN_W - MARGIN - 110, 12, 110, 40 }, ButtonStyle::Outline, COL_AMBER))
        app.screen = Screen::Profile;
    if (ui_button("Exam prep", { SCREEN_W - MARGIN - 110 - 12 - 120, 12, 120, 40 }, ButtonStyle::Outline, COL_PURPLE))
        app.screen = Screen::ExamPrep;

    float x = MARGIN, y = HEADER_H + 22;
    draw_course_mastery(app, x, y);
    y += 58;

    int number = 1;
    for (const std::string &track : track_names(app)) {
        draw_track(app, track, x, y, number);
        y += ROW_H;
    }

    draw_topic_card(app, { MARGIN + MAP_W + 24, HEADER_H + 22, SCREEN_W - 2 * MARGIN - MAP_W - 24, 470 });

    ui_footer_hints({ "<- -> choose topic", "UP DOWN change track", "ENTER lesson", "Q quiz", "E exam prep", "P profile", "ESC start screen" });
}
