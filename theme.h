// theme.h - colours and sizes, matching the Stack Trace mockup.
// Change a colour here and it changes everywhere.

#pragma once
#include "raylib.h"

const int SCREEN_W = 1280;
const int SCREEN_H = 720;

// Backgrounds and lines
const Color COL_BG        = { 13, 17, 23, 255 };
const Color COL_SURFACE   = { 22, 27, 34, 255 };
const Color COL_SURFACE_2 = { 28, 33, 40, 255 };
const Color COL_LINE      = { 48, 54, 61, 255 };
const Color COL_LINE_SOFT = { 33, 38, 45, 255 };

// Text
const Color COL_TEXT      = { 230, 237, 243, 255 };
const Color COL_TEXT_SOFT = { 201, 209, 217, 255 };
const Color COL_MUTED     = { 139, 148, 158, 255 };

// Meaning: green = right / mastered, amber = current / changed,
// coral = wrong / weak / leaked, blue and purple = pointers and tags
const Color COL_GREEN  = { 126, 231, 135, 255 };
const Color COL_AMBER  = { 255, 180, 84, 255 };
const Color COL_CORAL  = { 255, 123, 114, 255 };
const Color COL_BLUE   = { 121, 192, 255, 255 };
const Color COL_PURPLE = { 210, 168, 255, 255 };

// Shared layout
const int HEADER_H = 64;
const int FOOTER_H = 56;
const int MARGIN   = 40;
