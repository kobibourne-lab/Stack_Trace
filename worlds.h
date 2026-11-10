// worlds.h - the themed scenery behind each region of the campaign map, drawn
// entirely in code: Mario-style worlds, one per region.
//
//   0  green hills with towers of stacked frames      (The Stack)
//   1  a purple cave with floating memory addresses   (Address Space)
//   2  a moonlit sea of islands joined by chains      (The Heap)
//   3  a sunset city of castles                        (Object Land)
//
// A fifth region would reuse the first look, and so on.

#pragma once
#include "raylib.h"

// The world's highlight colour: its platforms, rings and paths.
Color world_accent(int world);

// True for the islands world, whose paths are drawn as chains of links.
bool world_has_chains(int world);

// Draw a world's scene filling `area` (clipped to it). t is the time, for the
// things that move: clouds, waves, flickering windows.
void world_scene(int world, Rectangle area, float t);

// A raised round platform, like a level dot on a Mario map, with its top
// centred on `top`. rx is its half-width.
void world_platform(Vector2 top, float rx, Color topColour);

// Paths: a chain of links (islands world) or a dotted line, from a to b.
void world_chain(Vector2 a, Vector2 b, Color colour);
