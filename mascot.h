// mascot.h - Mal, the heap block who lives in the machine's memory (Mal as in
// malloc). Mal is drawn entirely in code: rounded rectangles and circles, so
// it can squash, stretch, blink and crumble without any images.
//
// One Mal is shared by every screen (it lives in App), so a mood carries over
// when you change screen. Screens call mal_react when something happens and
// mal_draw where they want Mal; main.cpp calls mal_update once per frame.

#pragma once
#include "raylib.h"
#include <string>

enum class MalMood {
    Idle,      // looking around, blinking
    Happy,     // a right answer: hops, eyes closed in a smile
    Cheer,     // a combo or full marks: keeps bouncing
    Sad,       // a wrong answer: squashed, with a tear
    Worried,   // down to the last heart: wide eyes, a sweat drop
    Sleep,     // nothing practised today: eyes shut, zzz
    Freed,     // out of lives: free()'d, crumbles into memory cells
};

struct Mal {
    MalMood mood = MalMood::Idle;
    MalMood base = MalMood::Idle;   // what Mal goes back to after a reaction
    float moodLeft = 0;             // seconds until going back to base (0 = stay)

    float clock = 0;
    float squash = 1, squashVel = 0;   // 1 = normal; under 1 squashed, over 1 stretched
    float hop = 0, hopVel = 0;         // how far off the ground (negative = up)
    float blinkIn = 2, blinkLeft = 0;
    float crumble = 0;                 // 0 = whole, 1 = a pile of cells on the floor

    std::string hat = "none";          // from the wardrobe (wardrobe.h has the list)
    std::string skin = "classic";
    std::string pet = "nopet";         // cat, dog or bird
};

// React to something: show `mood` for `seconds`, then go back to the base
// mood. seconds = 0 keeps the mood until the next reaction.
void mal_react(Mal &m, MalMood mood, float seconds = 1.6f);

// The mood Mal settles into when nothing is happening.
void mal_set_base(Mal &m, MalMood mood);

void mal_update(Mal &m, float dt);

// Draw Mal standing with its feet at `feet`, `size` pixels tall (the body,
// not counting the antenna).
void mal_draw(const Mal &m, Vector2 feet, float size);

// Mal with their pet (if any) sitting beside them, for screens with room.
void mal_draw_with_pet(const Mal &m, Vector2 feet, float size);

// Just a pet, `size` pixels tall, sitting at `at`. tag = show its label
// (const, static, volatile).
void mal_draw_pet(const std::string &pet, Vector2 at, float size, float clock, bool tag);

// A speech bubble whose tail points at `tail`. The bubble's top-left corner is
// at (x, y), it's `width` wide, and as tall as the text needs.
void mal_say(const std::string &text, float x, float y, float width, Vector2 tail);
