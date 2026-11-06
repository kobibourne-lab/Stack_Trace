// ui.h - drawing pieces every screen shares: text, panels, buttons, code
// listings and the header bar. Buttons are "immediate mode": ui_button draws
// the button and returns true on the frame it's clicked.

#pragma once
#include <string>
#include <vector>
#include "raylib.h"
#include "theme.h"
#include "progress.h"

enum class FontKind { Ui, UiBold, Mono, MonoBold };

// Loads the fonts in fonts/. Missing files fall back to raylib's built-in font.
void ui_load_fonts();
void ui_unload_fonts();

// The screens are laid out for SCREEN_W x SCREEN_H and scaled up to fill the
// window (see main.cpp). main.cpp tells ui.cpp the scale each frame, so
// ui_scissor can clip in the right place.
void ui_set_fit(Camera2D fit);

// Use instead of BeginScissorMode: takes the box in screen layout coordinates
// (the same ones everything is drawn in) and clips to where it really is in
// the window. End it with EndScissorMode as usual.
void ui_scissor(int x, int y, int width, int height);

// Call at the start and end of each frame (main.cpp does). The end sets the
// mouse cursor to a hand when it's over anything clickable.
void ui_begin_frame();
void ui_end_frame();

// For clickable things that aren't ui_buttons (e.g. topic circles): show the hand cursor.
void ui_want_hand();
// Over editable text: show the text cursor (I-beam).
void ui_want_ibeam();

// Text centred on cx, wrapped to width, at most maxLines lines.
void ui_text_centered_wrapped(const std::string &text, float cx, float y, float width, float size,
                              Color color, int maxLines, FontKind font = FontKind::Ui);

// Text. Only plain ASCII characters are loaded, so avoid curly quotes and dashes.
void ui_text(const std::string &text, float x, float y, float size, Color color, FontKind font = FontKind::Ui);
float ui_text_width(const std::string &text, float size, FontKind font = FontKind::Ui);

// Wraps text to fit width ('\n' starts a new paragraph). Returns the height used.
float ui_text_wrapped(const std::string &text, float x, float y, float width, float size, Color color,
                      FontKind font = FontKind::Ui);

// The height ui_text_wrapped would use, without drawing anything.
float ui_text_wrapped_height(const std::string &text, float width, float size, FontKind font = FontKind::Ui);

// A rounded box with a border.
void ui_panel(Rectangle r, Color fill, Color border, float borderWidth = 1);

enum class ButtonStyle { Primary, Outline, Disabled };

// Draws a button; returns true when it's clicked. accent colours a Primary
// button's fill or an Outline button's border and label.
bool ui_button(const std::string &label, Rectangle r, ButtonStyle style = ButtonStyle::Outline,
               Color accent = COL_GREEN);

// The Stack Trace logo: a pointer p pointing at the newest stack frame, with a
// trace line climbing the frames. Drawn size x size with its top-left corner at
// (x, y). Below about 100 px the frame names are left out, since they'd be too
// small to read.
void ui_logo(float x, float y, float size);

// The bar across the top of every screen: the logo, then a subtitle.
void ui_header(const std::string &subtitle);

// A row of key hints along the bottom, e.g. {"ENTER open", "ESC back"}.
void ui_footer_hints(const std::vector<std::string> &hints);

// A code listing with line numbers. highlight (1 = first line, 0 = none) gets
// a coloured bar. If clickable, hovering lights a line and the clicked line
// number is returned (0 if nothing was clicked this frame).
int ui_code(const std::vector<std::string> &lines, Rectangle r, int highlight, Color highlightColor,
            bool clickable = false, float size = 16);

// Lives as hearts, filled or empty.
void ui_hearts(float x, float y, int lives, int maxLives);

// The RPG level badge: "LV 3", the level's title, and a thin bar of XP towards
// the next level. Drawn with its top-left at (x, y); returns how wide it was.
float ui_level_badge(float x, float y, int xp);
float ui_level_badge_width(int xp);

// Colours for the five mastery levels.
Color mastery_color(Mastery m);

// Small filled triangle that shows whichever way round the points are given.
void ui_triangle(Vector2 a, Vector2 b, Vector2 c, Color color);
