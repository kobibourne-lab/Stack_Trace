// fx.h - the little effects that make the app feel alive: bursts of particles,
// text that floats up and fades ("+10 XP"), and screen shake.
// Screens start effects; main.cpp updates and draws them once per frame, on
// top of whatever screen is showing.

#pragma once
#include "raylib.h"
#include <string>

// A burst of small squares and dots flying out from a point, like confetti made
// of memory cells. Two colours mix in the burst.
void fx_burst(Vector2 at, int count, Color a, Color b);

// Text that rises from `at` and fades out over about a second.
void fx_float_text(const std::string &text, Vector2 at, float size, Color color);

// Shake the whole screen. 0.3 is a nudge, 1 is a big hit. Shakes add up but
// are capped, and die away by themselves.
void fx_shake(float amount);

void fx_update(float dt);
void fx_draw();

// How far to move the screen this frame, for the shake (main.cpp adds this to
// the camera).
Vector2 fx_shake_offset();

// Easing for animations: t goes 0 to 1.
float ease_out_cubic(float t);
float ease_out_back(float t);   // overshoots a little, then settles: a "pop"
