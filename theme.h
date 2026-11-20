// theme.h - colours and sizes, matching the Stack Trace mockup.
// The colours belong to the app theme in use (apptheme.h): theme_apply()
// swaps them all at once, so change one in theme.cpp and it changes everywhere.

#pragma once
#include <string>
#include "raylib.h"

const int SCREEN_W = 1280;
const int SCREEN_H = 720;

// Backgrounds and lines
extern Color COL_BG;
extern Color COL_SURFACE;
extern Color COL_SURFACE_2;
extern Color COL_LINE;
extern Color COL_LINE_SOFT;

// Text
extern Color COL_TEXT;
extern Color COL_TEXT_SOFT;
extern Color COL_MUTED;

// Meaning: green = right / mastered, amber = current / changed,
// coral = wrong / weak / leaked, blue and purple = pointers and tags
extern Color COL_GREEN;
extern Color COL_AMBER;
extern Color COL_CORAL;
extern Color COL_BLUE;
extern Color COL_PURPLE;

// Shared layout
const int HEADER_H = 64;
const int FOOTER_H = 56;
const int MARGIN   = 40;

// ---------------------------------------------------------------------------
// The app themes (apptheme.h says what each is and what unlocks it)
// ---------------------------------------------------------------------------

// Switch every COL_ colour to a theme's ("default", "phosphor", "neon", "paper", "blueprint").
void theme_apply(const std::string &id);
const std::string &theme_current();

// Drawn behind every screen, after the background is cleared.
void theme_background(float t);
// ui_panel calls these around a panel: a glow or a shadow under it, tape,
// pencil lines or measurements over it. Only big panels get them.
void theme_panel_under(Rectangle r, Color fill, Color border);
void theme_panel_over(Rectangle r, Color fill, Color border);
// Drawn over everything at the end of a frame (the CRT's scanlines).
// titleBlock: Blueprint's drawing label, on the title screen only.
void theme_overlay(float t, bool titleBlock);

// A small picture of a theme, for the shop.
void theme_preview(const std::string &id, Rectangle r, float t);
