// sfx.cpp - builds each sound as a list of samples, then hands it to raylib.

#include "sfx.h"
#include "raylib.h"
#include <cmath>
#include <cstdlib>
#include <vector>

const int RATE = 44100;
const int SOUND_COUNT = 6;

static Sound sounds[SOUND_COUNT];
static bool ready = false;
static bool muted = false;

// One note: a square-ish wave (softened so it isn't harsh), with a quick fade
// in and a fade out, added onto the samples starting at `start` seconds.
static void add_note(std::vector<float> &out, float start, float length, float freq, float volume, float slide = 0)
{
    int from = (int)(start * RATE), count = (int)(length * RATE);
    if ((int)out.size() < from + count) out.resize(from + count, 0.0f);
    float phase = 0;
    for (int i = 0; i < count; i++) {
        float t = (float)i / count;
        float f = freq + slide * t;                    // slide: the pitch drifts over the note
        phase += f / RATE;
        float wave = tanhf(3.0f * sinf(2 * PI * phase)) * 0.8f;   // between a sine and a square
        float env = fminf(1.0f, i / (0.004f * RATE)) * powf(1.0f - t, 1.6f);
        out[from + i] += wave * env * volume;
    }
}

static void add_noise(std::vector<float> &out, float start, float length, float volume)
{
    int from = (int)(start * RATE), count = (int)(length * RATE);
    if ((int)out.size() < from + count) out.resize(from + count, 0.0f);
    float held = 0;
    for (int i = 0; i < count; i++) {
        if (i % 6 == 0) held = (rand() / (float)RAND_MAX) * 2 - 1;   // a crunchy, bit-crushed noise
        float t = (float)i / count;
        out[from + i] += held * powf(1.0f - t, 2.0f) * volume;
    }
}

static Sound make_sound(const std::vector<float> &samples)
{
    std::vector<short> pcm(samples.size());
    for (size_t i = 0; i < samples.size(); i++) {
        float v = fmaxf(-1.0f, fminf(1.0f, samples[i]));
        pcm[i] = (short)(v * 32000);
    }
    Wave w = {};
    w.frameCount = (unsigned int)pcm.size();
    w.sampleRate = RATE;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = pcm.data();
    return LoadSoundFromWave(w);   // copies the samples, so pcm can go
}

void sfx_init()
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;   // no sound card: stay silent
    std::vector<float> s;

    s.clear(); add_note(s, 0, 0.03f, 1400, 0.25f);                              // Click
    sounds[(int)Sfx::Click] = make_sound(s);

    s.clear(); add_note(s, 0, 0.09f, 784, 0.35f); add_note(s, 0.08f, 0.16f, 1175, 0.35f);   // Correct: G5, D6
    sounds[(int)Sfx::Correct] = make_sound(s);

    s.clear();                                                                 // Combo: C6 E6 G6 C7
    add_note(s, 0, 0.07f, 1047, 0.3f); add_note(s, 0.06f, 0.07f, 1319, 0.3f);
    add_note(s, 0.12f, 0.07f, 1568, 0.3f); add_note(s, 0.18f, 0.2f, 2093, 0.3f);
    sounds[(int)Sfx::Combo] = make_sound(s);

    s.clear(); add_note(s, 0, 0.28f, 180, 0.45f, -70); add_note(s, 0, 0.28f, 186, 0.25f, -70);   // Wrong
    sounds[(int)Sfx::Wrong] = make_sound(s);

    s.clear();                                                                 // LevelUp: G5 C6 E6, G6 held
    add_note(s, 0, 0.1f, 784, 0.3f); add_note(s, 0.1f, 0.1f, 1047, 0.3f);
    add_note(s, 0.2f, 0.1f, 1319, 0.3f); add_note(s, 0.3f, 0.45f, 1568, 0.32f);
    add_note(s, 0.3f, 0.45f, 784, 0.18f);
    sounds[(int)Sfx::LevelUp] = make_sound(s);

    s.clear(); add_noise(s, 0, 0.6f, 0.4f); add_note(s, 0, 0.35f, 300, 0.25f, -220);   // Crumble
    sounds[(int)Sfx::Crumble] = make_sound(s);

    ready = true;
}

void sfx_play(Sfx which)
{
    if (!ready || muted) return;
    PlaySound(sounds[(int)which]);
}

void sfx_toggle_mute() { muted = !muted; }
bool sfx_muted() { return muted; }

void sfx_close()
{
    if (ready)
        for (Sound &s : sounds) UnloadSound(s);
    ready = false;
    if (IsAudioDeviceReady()) CloseAudioDevice();
}
