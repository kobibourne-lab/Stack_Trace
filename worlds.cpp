// worlds.cpp - drawing the campaign's themed worlds.

#include "worlds.h"
#include "theme.h"
#include "ui.h"
#include <cmath>
#include <cstdio>

enum class Look { Plains, Caverns, Islands, Citadel, Grove, Library };

// The two later worlds' highlights: a firefly mint and a reading-lamp rose.
const Color GROVE_MINT = { 110, 230, 200, 255 };
const Color LIBRARY_ROSE = { 245, 140, 180, 255 };

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
    { Look::Grove,   { 6, 22, 24, 255 },  { 18, 58, 52, 255 }, { 14, 44, 42, 255 }, { 10, 34, 32, 255 },
      GROVE_MINT, { 220, 255, 150, 255 } },
    { Look::Library, { 26, 16, 12, 255 }, { 66, 42, 26, 255 }, { 50, 32, 22, 255 }, { 36, 24, 16, 255 },
      LIBRARY_ROSE, { 255, 228, 190, 255 } },
};
const int LOOK_COUNT = sizeof LOOKS / sizeof LOOKS[0];

static const WorldLook &look_for(int world) { return LOOKS[((world % LOOK_COUNT) + LOOK_COUNT) % LOOK_COUNT]; }

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
        if (L.look == Look::Grove) {   // fireflies wander about
            x += sinf(t * (0.5f + hash01(seed + i * 13)) + i) * 14;
            y += cosf(t * (0.4f + hash01(seed + i * 17)) + i * 2) * 10;
        }
        if (L.look == Look::Library)   // dust drifting down through the lamplight
            y = a.y + fmodf(t * (4 + hash01(seed + i * 5) * 6) + hash01(seed + i) * a.height, a.height);
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

    case Look::Grove: {
        ridge(a, a.y + h * 0.5f, 60, 3.0f + world, L.far, false);
        for (int i = 0; i < (int)(w / 26) + 2; i++) {   // a dark wall of pines behind
            float x = a.x + i * 26 - 10 + hash01(seed + 800 + i) * 10;
            float base = a.y + h * 0.62f, th = 60 + hash01(seed + 810 + i) * 60;
            for (int k = 0; k < 3; k++) {
                float ty = base - th * (0.25f + k * 0.28f), tw = 20 - k * 4;
                DrawTriangle({ x + tw, ty + th * 0.35f }, { x, ty - th * 0.08f }, { x - tw, ty + th * 0.35f }, shade(L.far, 0.85f));
            }
        }
        for (int i = 0; i < 2; i++) {   // family trees: a base class with two derived classes growing from it
            float x = a.x + w * (0.25f + i * 0.5f) + (hash01(seed + 820 + i) - 0.5f) * 30;
            float ground = a.y + h * 0.8f, top = ground - 120 - i * 20;
            DrawRectangle((int)(x - 5), (int)top, 10, (int)(ground - top), shade(L.near, 1.4f));
            Vector2 base = { x, top }, kids[2] = { { x - 44, top + 40 }, { x + 44, top + 40 } };
            for (const Vector2 &k : kids) DrawLineEx({ x, top + 14 }, k, 5, shade(L.near, 1.4f));
            float glow = 0.55f + 0.25f * sinf(t * 1.3f + i * 2);
            DrawRectangleRounded({ base.x - 26, base.y - 22, 52, 26 }, 0.35f, 6, Fade(L.accent, 0.5f * glow));
            DrawRectangleRoundedLinesEx({ base.x - 26, base.y - 22, 52, 26 }, 0.35f, 6, 1.5f, Fade(L.accent, 0.8f));
            for (const Vector2 &k : kids) {
                DrawRectangleRounded({ k.x - 20, k.y - 8, 40, 20 }, 0.35f, 6, Fade(L.accent, 0.3f * glow));
                DrawRectangleRoundedLinesEx({ k.x - 20, k.y - 8, 40, 20 }, 0.35f, 6, 1.5f, Fade(L.accent, 0.6f));
            }
        }
        ridge(a, a.y + h * 0.9f, 55, 9.0f + world, L.near, false);
        break;
    }

    case Look::Library: {
        for (int i = 0; i < 3; i++) {   // tall arched windows, moonlight coming in
            float x = a.x + w * (0.18f + i * 0.32f), y = a.y + 70, ww = 46, wh = 120;
            DrawRectangle((int)(x - ww / 2), (int)(y + ww / 2), (int)ww, (int)(wh - ww / 2), Fade(L.sparkle, 0.07f));
            DrawCircleV({ x, y + ww / 2 }, ww / 2, Fade(L.sparkle, 0.07f));
            DrawLineEx({ x, y }, { x, y + wh }, 2, Fade(L.far, 0.9f));
            DrawLineEx({ x - ww / 2, y + wh * 0.6f }, { x + ww / 2, y + wh * 0.6f }, 2, Fade(L.far, 0.9f));
        }
        static const Color spines[] = { { 160, 70, 60, 255 }, { 70, 110, 150, 255 }, { 200, 160, 80, 255 }, { 90, 130, 90, 255 }, { 130, 90, 150, 255 } };
        float shelfTop = a.y + h * 0.42f;
        for (int row = 0; row < 5; row++) {   // shelves of books right across, the containers of the STL
            float y = shelfTop + row * 46;
            DrawRectangle((int)a.x, (int)(y + 36), (int)w, 6, shade(L.far, 1.5f));
            float x = a.x + hash01(seed + 900 + row) * 10;
            int k = 0;
            while (x < a.x + w) {
                float bw = 7 + hash01(seed + row * 97 + k) * 9, bh = 24 + hash01(seed + row * 53 + k) * 12;
                if (hash01(seed + row * 31 + k) > 0.12f)
                    DrawRectangle((int)x, (int)(y + 36 - bh), (int)bw - 1, (int)bh, shade(spines[(row + k) % 5], 0.55f));
                x += bw;
                k++;
            }
        }
        static const char *words[] = { "<T>", "vector", "map", "list", "begin()", "end()" };
        for (int i = 0; i < 6; i++) {   // template names drifting up between the shelves
            float x = a.x + hash01(seed + 950 + i) * (w - 60);
            float y = a.y + h - fmodf(t * (7 + hash01(seed + 960 + i) * 8) + hash01(seed + 970 + i) * h, h);
            ui_text(words[i], x, y, 12, Fade(L.accent, 0.3f), FontKind::Mono);
        }
        DrawRectangleGradientV((int)a.x, (int)(a.y + h * 0.9f), (int)w, (int)(h * 0.1f) + 1, Fade(L.near, 0.0f), L.near);
        break;
    }
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
