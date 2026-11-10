// worlds.cpp - drawing the campaign's themed worlds.

#include "worlds.h"
#include "theme.h"
#include "ui.h"
#include <cmath>
#include <cstdio>

enum class Look { Plains, Caverns, Islands, Citadel };

struct WorldLook {
    Look look;
    Color skyTop, skyBottom, far, near, accent, sparkle;
};

static const WorldLook LOOKS[] = {
    { Look::Plains,  { 10, 26, 30, 255 }, { 18, 56, 46, 255 }, { 22, 64, 50, 255 }, { 28, 84, 60, 255 },
      COL_GREEN,  { 190, 255, 200, 255 } },
    { Look::Caverns, { 16, 12, 30, 255 }, { 40, 28, 68, 255 }, { 30, 22, 54, 255 }, { 46, 34, 80, 255 },
      COL_PURPLE, { 225, 200, 255, 255 } },
    { Look::Islands, { 8, 20, 40, 255 },  { 22, 54, 90, 255 }, { 18, 44, 72, 255 }, { 14, 40, 70, 255 },
      COL_BLUE,   { 200, 230, 255, 255 } },
    { Look::Citadel, { 34, 16, 20, 255 }, { 96, 52, 30, 255 }, { 58, 30, 26, 255 }, { 76, 40, 30, 255 },
      COL_AMBER,  { 255, 220, 160, 255 } },
};

static const WorldLook &look_for(int world) { return LOOKS[((world % 4) + 4) % 4]; }

Color world_accent(int world) { return look_for(world).accent; }
bool world_has_chains(int world) { return look_for(world).look == Look::Islands; }

static Color shade(Color c, float f)
{
    auto ch = [&](unsigned char v) { float x = v * f; return (unsigned char)(x > 255 ? 255 : x); };
    return { ch(c.r), ch(c.g), ch(c.b), c.a };
}

// A number from 0 to 1 that's always the same for the same n, so scenery
// stays put from frame to frame.
static float hash01(int n)
{
    unsigned int x = (unsigned int)n * 2654435761u + 0x9e3779b9u;
    x ^= x >> 15; x *= 2246822519u; x ^= x >> 13;
    return (x & 0xffff) / 65535.0f;
}

static float hills(float x, float seed)
{
    return sinf(x * 0.012f + seed) * 0.5f + sinf(x * 0.031f + seed * 2.3f) * 0.3f + sinf(x * 0.07f + seed * 5.1f) * 0.2f;
}

// A filled skyline of thin columns across the area: reads as smooth hills
// (or jagged rock).
static void ridge(Rectangle a, float baseY, float height, float seed, Color c, bool jagged)
{
    const int step = 3;
    for (float x = a.x; x < a.x + a.width; x += step) {
        float h = hills(x, seed);
        if (jagged) h = h * 0.6f + (hash01((int)(x / 16) + (int)(seed * 977)) - 0.5f) * 0.9f;
        float top = baseY - height * (0.5f + 0.5f * h);
        DrawRectangle((int)x, (int)top, step + 1, (int)(a.y + a.height - top), c);
    }
}

void world_scene(int world, Rectangle a, float t)
{
    const WorldLook &L = look_for(world);
    ui_scissor((int)a.x, (int)a.y, (int)a.width, (int)a.height);
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, (int)a.height, L.skyTop, L.skyBottom);
    int seed = world * 1000;

    // Little lights: stars, spores, bubbles or embers
    for (int i = 0; i < 18; i++) {
        float x = a.x + hash01(seed + i * 3) * a.width;
        float y = a.y + 60 + hash01(seed + i * 7) * a.height * 0.6f;
        if (L.look == Look::Caverns || L.look == Look::Citadel)   // these drift upwards
            y = a.y + a.height - fmodf(t * (8 + hash01(seed + i * 5) * 12) + hash01(seed + i) * a.height, a.height);
        float tw = 0.5f + 0.5f * sinf(t * (1.2f + hash01(seed + i) * 2) + i);
        DrawCircleV({ x, y }, 1.1f + hash01(seed + i * 11) * 1.4f, Fade(L.sparkle, 0.2f + 0.5f * tw));
    }

    float w = a.width, h = a.height;
    switch (L.look) {
    case Look::Plains:
        for (int i = 0; i < 2; i++) {   // clouds drifting across
            float cw = 70 + hash01(seed + 40 + i) * 50;
            float x = a.x + fmodf(hash01(seed + 30 + i) * (w + 200) + t * (6 + i * 3), w + 200) - 100;
            float y = a.y + 80 + i * 60;
            DrawRectangleRounded({ x, y, cw, 20 }, 1, 8, Fade(L.sparkle, 0.07f));
            DrawRectangleRounded({ x + cw * 0.2f, y - 11, cw * 0.5f, 22 }, 1, 8, Fade(L.sparkle, 0.07f));
        }
        ridge(a, a.y + h * 0.55f, 70, 1.0f + world, L.far, false);
        for (int i = 0; i < 3; i++) {   // towers of stacked frames, in the distance
            float x = a.x + 14 + i * (w / 3) + hash01(seed + 7 + i) * 30;
            int blocks = 2 + (int)(hash01(seed + 70 + i) * 4);
            float base = a.y + h * 0.55f - 20;
            for (int b = 0; b < blocks; b++) {
                Rectangle r = { x, base - (b + 1) * 16, 40, 13 };
                DrawRectangleRounded(r, 0.3f, 6, shade(L.far, 1.35f + b * 0.06f));
                if (b == blocks - 1) DrawRectangleRounded({ r.x, r.y, r.width, 4 }, 0.5f, 6, Fade(L.accent, 0.35f));
            }
        }
        ridge(a, a.y + h * 0.86f, 70, 4.0f + world, L.near, false);
        break;

    case Look::Caverns:
        ridge(a, a.y + h * 0.66f, 110, 2.0f + world, L.far, true);
        for (int i = 0; i < (int)(w / 34) + 1; i++) {   // stalactites hanging from the roof
            float x = a.x + i * 34 + hash01(seed + 300 + i) * 18;
            float len = 30 + hash01(seed + 310 + i) * 70, sw = 14 + hash01(seed + 320 + i) * 18;
            DrawTriangle({ x - sw / 2, a.y }, { x, a.y + len }, { x + sw / 2, a.y }, L.far);
        }
        for (int i = 0; i < 4; i++) {   // glowing crystals, pulsing
            float x = a.x + 30 + i * (w - 60) / 3;
            float y = a.y + h * 0.9f - hash01(seed + 410 + i) * 40;
            float ch = 20 + hash01(seed + 420 + i) * 22, glow = 0.5f + 0.5f * sinf(t * 1.6f + i);
            DrawTriangle({ x + 7, y }, { x, y - ch }, { x - 7, y }, Fade(L.accent, 0.5f + 0.35f * glow));
        }
        for (int i = 0; i < 5; i++) {   // memory addresses floating up
            float x = a.x + hash01(seed + 500 + i) * (w - 50);
            float y = a.y + a.height - fmodf(t * (9 + hash01(seed + 510 + i) * 10) + hash01(seed + 520 + i) * h, h);
            char addr[16];
            snprintf(addr, sizeof addr, "0x%04X", (int)(hash01(seed + 530 + i) * 0xffff));
            ui_text(addr, x, y, 11, Fade(L.sparkle, 0.2f), FontKind::Mono);
        }
        ridge(a, a.y + h * 0.95f, 50, 6.0f + world, L.near, true);
        break;

    case Look::Islands: {
        Vector2 moon = { a.x + w * 0.78f, a.y + 92 };
        DrawCircleV(moon, 30, Fade(L.sparkle, 0.12f));
        DrawCircleV(moon, 21, Fade(L.sparkle, 0.55f));
        float sea = a.y + h * 0.36f;
        for (int i = 0; i < 3; i++)   // islands on the horizon
            DrawEllipse((int)(a.x + 30 + i * w / 3 + hash01(seed + 600 + i) * 30), (int)sea,
                        30 + hash01(seed + 610 + i) * 30, 9 + hash01(seed + 620 + i) * 8, L.far);
        DrawRectangleGradientV((int)a.x, (int)sea, (int)w, (int)(a.y + h - sea), L.far, shade(L.near, 0.8f));
        for (int row = 0; row < 14; row++) {   // waves rolling along
            float y = sea + 14 + row * 23;
            for (int i = 0; i < 4; i++) {
                float x = a.x + fmodf(i * (w / 3) + hash01(seed + row * 13 + i) * 60 + t * (10 + row * 2), w + 60) - 40;
                DrawRectangleRounded({ x, y, 26 + row * 1.5f, 3 }, 1, 4, Fade(L.sparkle, 0.10f + row * 0.006f));
            }
        }
        break;
    }

    case Look::Citadel:
        DrawCircleV({ a.x + w * 0.5f, a.y + h * 0.5f }, 100, Fade(L.accent, 0.10f));   // a big low sun
        DrawCircleV({ a.x + w * 0.5f, a.y + h * 0.5f }, 68, Fade(L.accent, 0.20f));
        for (int i = 0; i < (int)(w / 70) + 1; i++) {   // a skyline of class buildings
            float x = a.x + i * 70 + hash01(seed + 700 + i) * 20 - 10;
            float bh = 80 + hash01(seed + 710 + i) * 110, bw = 48 + hash01(seed + 720 + i) * 22;
            float top = a.y + h * 0.62f - bh;
            DrawRectangle((int)x, (int)top, (int)bw, (int)(a.y + h - top), L.far);
            for (int c = 0; c < (int)(bw / 12); c += 2) DrawRectangle((int)(x + c * 12), (int)(top - 8), 8, 8, L.far);
            if (hash01(seed + 730 + i) > 0.5f) {   // a tower with a pointed roof
                float tx = x + bw * 0.3f;
                DrawRectangle((int)tx, (int)(top - 36), 18, 36, L.far);
                DrawTriangle({ tx + 23, top - 36 }, { tx + 9, top - 64 }, { tx - 5, top - 36 }, L.far);
            }
            for (int wy = 0; wy < 4; wy++)
                for (int wx = 0; wx < 2; wx++) {
                    if (hash01(seed + i * 31 + wy * 7 + wx) < 0.45f) continue;
                    float flick = 0.55f + 0.45f * sinf(t * 2 + i + wy);
                    DrawRectangle((int)(x + 10 + wx * 20), (int)(top + 14 + wy * 22), 7, 10, Fade(L.accent, 0.35f * flick));
                }
        }
        ridge(a, a.y + h * 0.93f, 60, 8.0f + world, L.near, false);
        break;
    }

    // Darken the top, where the region's name sits, so it always reads
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, 76, Fade(COL_BG, 0.85f), Fade(COL_BG, 0.0f));
    EndScissorMode();
}

void world_platform(Vector2 top, float rx, Color topColour)
{
    float ry = rx * 0.38f, h = rx * 0.42f;
    Color side = shade(topColour, 0.55f);
    DrawEllipse((int)top.x, (int)(top.y + h + 5), rx + 3, ry + 2, Fade(BLACK, 0.4f));   // shadow
    DrawEllipse((int)top.x, (int)(top.y + h), rx, ry, side);
    DrawRectangle((int)(top.x - rx), (int)top.y, (int)(rx * 2), (int)h, side);
    DrawEllipse((int)top.x, (int)top.y, rx, ry, topColour);
    DrawEllipse((int)top.x, (int)(top.y - 1.5f), rx * 0.78f, ry * 0.62f, shade(topColour, 1.12f));   // a highlight
}

void world_chain(Vector2 a, Vector2 b, Color colour)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx * dx + dy * dy);
    for (float d = 6; d <= len - 6; d += 11)
        DrawRing({ a.x + dx * d / len, a.y + dy * d / len }, 3, 5.5f, 0, 360, 12, colour);
}
