// sfx.h - the app's sound effects. They're made in code when the app starts
// (short 8-bit style beeps), so there are no sound files to find or lose.
// If the computer has no sound device, everything here quietly does nothing.

#pragma once

enum class Sfx {
    Click,     // any button
    Correct,   // a right answer: two quick rising notes
    Combo,     // 3 or more right in a row: a brighter run of notes
    Wrong,     // a wrong answer: a low buzz that drops
    LevelUp,   // a little fanfare
    Crumble,   // Mal being free()'d: a burst of noise that fades
};

void sfx_init();
void sfx_play(Sfx which);
void sfx_toggle_mute();    // M on any screen
bool sfx_muted();
void sfx_close();
