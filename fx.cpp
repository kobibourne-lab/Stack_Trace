// fx.cpp - particles, floating text and screen shake.

#include "fx.h"
#include "ui.h"
#include <cmath>
#include <cstdlib>
#include <vector>

struct Particle {
    Vector2 pos, vel;
    float life, maxLife, size, spin, angle;
    Color color;
    bool square;
};

struct FloatText {
    std::string text;
    Vector2 pos;
    float life, maxLife, size;
    Color color;
};

static std::vector<Particle> particles;
static std::vector<FloatText> texts;
static float trauma = 0;   // 0 to 1; the shake is trauma squared, so it fades smoothly
static float shakeTime = 0;

static float frand(float lo, float hi) { return lo + (hi - lo) * (rand() / (float)RAND_MAX); }

void fx_burst(Vector2 at, int count, Color a, Color b)
{
    for (int i = 0; i < count; i++) {
        float angle = frand(0, 2 * PI), speed = frand(120, 420);
        Particle p;
        p.pos = at;
        p.vel = { cosf(angle) * speed, sinf(angle) * speed - 140 };   // a little upward kick
        p.maxLife = p.life = frand(0.5f, 0.95f);
        p.size = frand(4, 9);
        p.spin = frand(-8, 8);
        p.angle = frand(0, 360);
        p.color = (i % 3 == 0) ? b : a;
        p.square = (i % 2 == 0);
        particles.push_back(p);
    }
}

void fx_float_text(const std::string &text, Vector2 at, float size, Color color)
{
    texts.push_back({ text, at, 1.1f, 1.1f, size, color });
}

void fx_shake(float amount)
{
    trauma = fminf(1.0f, trauma + amount);
}

void fx_update(float dt)
{
    for (Particle &p : particles) {
        p.life -= dt;
        p.vel.y += 700 * dt;            // gravity
        p.vel.x *= 1.0f - 1.5f * dt;    // air
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.angle += p.spin * 60 * dt;
    }
    for (size_t i = 0; i < particles.size();)
        if (particles[i].life <= 0) particles.erase(particles.begin() + i); else i++;

    for (FloatText &t : texts) {
        t.life -= dt;
        t.pos.y -= 55 * dt;
    }
    for (size_t i = 0; i < texts.size();)
        if (texts[i].life <= 0) texts.erase(texts.begin() + i); else i++;

    trauma = fmaxf(0.0f, trauma - 1.8f * dt);
    shakeTime += dt;
}

void fx_draw()
{
    for (const Particle &p : particles) {
        float a = fminf(1.0f, p.life / (p.maxLife * 0.5f));
        Color c = Fade(p.color, a);
        if (p.square)
            DrawRectanglePro({ p.pos.x, p.pos.y, p.size, p.size }, { p.size / 2, p.size / 2 }, p.angle, c);
        else
            DrawCircleV(p.pos, p.size * 0.45f, c);
    }
    for (const FloatText &t : texts) {
        float age = 1.0f - t.life / t.maxLife;              // 0 to 1
        float grow = ease_out_back(fminf(1.0f, age * 4));   // pops in during the first quarter
        float a = age < 0.6f ? 1.0f : 1.0f - (age - 0.6f) / 0.4f;
        float size = t.size * (0.6f + 0.4f * grow);
        float w = ui_text_width(t.text, size, FontKind::MonoBold);
        ui_text(t.text, t.pos.x - w / 2 + 2, t.pos.y + 2, size, Fade(BLACK, a * 0.5f), FontKind::MonoBold);
        ui_text(t.text, t.pos.x - w / 2, t.pos.y, size, Fade(t.color, a), FontKind::MonoBold);
    }
}

Vector2 fx_shake_offset()
{
    float s = trauma * trauma * 14;   // up to 14 px
    return { s * sinf(shakeTime * 73), s * sinf(shakeTime * 91 + 1.3f) };
}

float ease_out_cubic(float t)
{
    t = fminf(1.0f, fmaxf(0.0f, t));
    float u = 1 - t;
    return 1 - u * u * u;
}

float ease_out_back(float t)
{
    t = fminf(1.0f, fmaxf(0.0f, t));
    const float c1 = 1.70158f, c3 = c1 + 1;
    float u = t - 1;
    return 1 + c3 * u * u * u + c1 * u * u;
}
