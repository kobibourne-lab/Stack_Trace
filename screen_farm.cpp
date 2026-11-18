// screen_farm.cpp - the Bit Garden (the Memory Farm), out of the window in
// Mal's room. A slab of memory cells floats in a synthwave world: a sun with
// scrolling stripes, a neon grid rushing towards you, and on some days
// mountains or the northern lights. The look changes every day of the week
// (garden_theme_for in farm.cpp): Monday Midnight Ice ... Sunday Aurora.
//
// Each topic is a cell with a plant made of glowing cubes. It grows with the
// topic's mastery, and gets BIT ROT (cubes go grey, crumble and float off as
// 0s and 1s) when the topic needs reviewing: click it to quiz it. Plants in
// fruit can be harvested for bytes, and the gc bot (the garbage collector,
// bought with bytes) does the rounds and harvests for you, with gc.c running
// in the corner. The code tree, main(), stands in the middle.
// The rules are in farm.cpp; this file only draws them and handles clicks.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include "rlgl.h"
#include <cmath>
#include <vector>
#include <algorithm>

const int GRID = 5;                         // 5 x 5 cells, the middle one is the tree
const float TILE_W = 150, TILE_H = 75;      // a cell's diamond
const Vector2 CENTRE = { 620, 455 };        // the middle of the slab (the tree)
const float SLAB_DEPTH = 40;
const float HORIZON = 340;

static bool treeOpen = false;
static float botT = 0;            // where the gc bot is: cell index + how far to the next
static int botLine = 0;           // the line of gc.c running now
static int botDone = 0;           // cells visited this lap
static int previewTheme = -1;     // -1 = today's look; the arrows preview the other days

// ---------------------------------------------------------------------------
// The seven looks
// ---------------------------------------------------------------------------

struct GardenTheme {
    unsigned int skyTop, skyMid, skyBot, sunTop, sunBot, stripe, floor, grid, horizon;
    bool mountains;
    unsigned int mountainFill, mountainEdge;
    bool aurora;
    unsigned int slab, side, edge, stem, leaf, fruit, trunk, canopy, treeFruit, accent, coin;
};

static const GardenTheme THEMES[GARDEN_THEMES] = {
    // Monday: Midnight Ice
    { 0x000814, 0x001d3d, 0x0a4b8c, 0xffffff, 0x7ad7ff, 0x00152c, 0x000814, 0x4cc9f0, 0x7ad7ff, true, 0x000c1c, 0x4cc9f0, false,
      0x03142b, 0x021026, 0x4cc9f0, 0x4cc9f0, 0xb5f1ff, 0xffffff, 0x3a86ff, 0x90e0ef, 0xffffff, 0x4cc9f0, 0xb5f1ff },
    // Tuesday: Vaporwave
    { 0x2b1055, 0xd16ba5, 0xffd6e8, 0xfffbd5, 0xff9ecd, 0x7a3e8e, 0x1d0f3a, 0x86fff4, 0xff9ecd, false, 0, 0, false,
      0x2a1650, 0x24124a, 0x86fff4, 0x5ce1d6, 0xa8ffef, 0xff9ecd, 0x9d7bff, 0xc3a6ff, 0xff9ecd, 0x86fff4, 0xff9ecd },
    // Wednesday: Toxic Lime
    { 0x020a02, 0x12301a, 0x7cff4f, 0xf4ff80, 0x39ff14, 0x0b2210, 0x030a04, 0xb026ff, 0x39ff14, false, 0, 0, false,
      0x140426, 0x110322, 0xb026ff, 0x39ff14, 0xa6ff4d, 0xff2bd6, 0x7a1fd6, 0x39ff14, 0xff2bd6, 0xb026ff, 0x39ff14 },
    // Thursday: Gold Hour
    { 0x03161a, 0x0b4d52, 0xffb347, 0xfff3b0, 0xff8c1a, 0x06363a, 0x021012, 0xffd166, 0xffb347, true, 0x021c1f, 0xffd166, false,
      0x062a2e, 0x05242a, 0xffd166, 0x2ec4b6, 0x7ae7c7, 0xffd166, 0xcb997e, 0x2ec4b6, 0xffd166, 0xffd166, 0xffb347 },
    // Friday: Outrun
    { 0x0a0010, 0x4a0026, 0xff3b3b, 0xfff15c, 0xff2a2a, 0x2a0012, 0x0a0006, 0xff2bd6, 0xff3b3b, true, 0x14000c, 0xff2bd6, false,
      0x1a0012, 0x160010, 0xff2bd6, 0xff6a3d, 0xffb03b, 0xfff15c, 0xc2185b, 0xff2bd6, 0xfff15c, 0xff2bd6, 0xfff15c },
    // Saturday: Synthwave
    { 0x12002e, 0x5a0f63, 0xff5f7e, 0xffe066, 0xff4d8d, 0x3b0a4a, 0x0b0220, 0x00e5ff, 0xff4d8d, false, 0, 0, false,
      0x1b0538, 0x170336, 0x00e5ff, 0x00b3c7, 0x38f0ff, 0xff4d8d, 0x00b3c7, 0x00e5ff, 0xff4d8d, 0x00e5ff, 0xff4d8d },
    // Sunday: Aurora
    { 0x010a12, 0x06283a, 0x0f5c5a, 0xe9fff6, 0x2dffb3, 0x05323a, 0x020a10, 0x2dffb3, 0x2dffb3, true, 0x03141c, 0x2de2ff, true,
      0x05202a, 0x041a24, 0x2dffb3, 0x2de2ff, 0x9ffcff, 0xff8bd1, 0x1f7a8c, 0x2dffb3, 0xff8bd1, 0x2dffb3, 0xff8bd1 },
};

static Color hex(unsigned int c, float alpha = 1)
{
    return { (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c, (unsigned char)(255 * alpha) };
}

static Color shade(Color c, float f)
{
    auto ch = [f](unsigned char v) { float x = v * f; return (unsigned char)(x > 255 ? 255 : x); };
    return { ch(c.r), ch(c.g), ch(c.b), c.a };
}

static Color mixc(Color a, Color b, float u)
{
    return { (unsigned char)(a.r + (b.r - a.r) * u), (unsigned char)(a.g + (b.g - a.g) * u), (unsigned char)(a.b + (b.b - a.b) * u),
             (unsigned char)(a.a + (b.a - a.a) * u) };
}

// A number from 0 to 1 that's always the same for the same seed (for stars, mountains...).
static float noise(int seed)
{
    unsigned int h = (unsigned int)seed * 2654435761u;
    h ^= h >> 13;
    h *= 2246822519u;
    h ^= h >> 16;
    return (h & 0xffff) / 65535.0f;
}

// ---------------------------------------------------------------------------
// The world behind the slab: sky, sun, mountains, aurora, the neon grid
// ---------------------------------------------------------------------------

static void draw_backdrop(const GardenTheme &th, float t)
{
    // Sky
    DrawRectangleGradientV(0, 0, SCREEN_W, (int)(HORIZON * 0.55f), hex(th.skyTop), hex(th.skyMid));
    DrawRectangleGradientV(0, (int)(HORIZON * 0.55f), SCREEN_W, (int)(HORIZON * 0.45f) + 1, hex(th.skyMid), hex(th.skyBot));
    for (int i = 0; i < 60; i++) {   // stars, twinkling
        float x = noise(i * 3 + 1) * SCREEN_W, y = noise(i * 3 + 2) * HORIZON * 0.6f;
        float tw = 0.4f + 0.6f * fabsf(sinf(t * (0.6f + noise(i) * 1.5f) + i));
        DrawCircleV({ x, y }, 0.6f + noise(i * 7) * 1.2f, Fade(WHITE, tw * 0.8f));
    }

    // Northern lights: three wavy curtains drifting
    if (th.aurora) {
        unsigned int colours[3] = { 0x2dffb3, 0x7a5cff, 0x2de2ff };
        for (int k = 0; k < 3; k++) {
            float base = 70 + k * 40.0f;
            for (int x = 0; x < SCREEN_W; x += 6) {
                float y = base + 38 * sinf(x * 0.006f + t * 0.35f + k * 2) + 18 * sinf(x * 0.013f - t * 0.25f + k);
                float a = (0.16f + 0.1f * sinf(x * 0.02f + t + k)) * (k == 1 ? 0.8f : 1);
                DrawRectangleGradientV(x, (int)y, 6, 110, hex(colours[k], a), hex(colours[k], 0));
            }
        }
    }

    // The sun: a gradient disc, cut off by the horizon, with dark stripes scrolling down it
    Vector2 sc = { 640, 300 };
    float r = 170;
    for (int i = 4; i >= 1; i--) DrawCircleV(sc, r + i * 18, hex(th.sunBot, 0.05f));
    for (int y = (int)(sc.y - r); y < (int)HORIZON; y++) {
        float dy = y - sc.y, half = sqrtf(fmaxf(0, r * r - dy * dy));
        Color c = mixc(hex(th.sunTop), hex(th.sunBot), (y - (sc.y - r)) / (HORIZON - (sc.y - r)));
        DrawRectangle((int)(sc.x - half), y, (int)(half * 2), 1, c);
    }
    float scroll = fmodf(t * 8, 15);
    for (int k = -1; k < 9; k++) {
        float yy = 220 + k * 15 + scroll;
        if (yy < 214 || yy > HORIZON) continue;
        float h = 2 + (yy - 214) / 15 * 1.3f;
        DrawRectangle(440, (int)yy, 400, (int)h, hex(th.stripe));
    }

    // Mountains along the horizon (low in the middle, so the sun shows)
    if (th.mountains) {
        std::vector<Vector2> pts;
        float x = 0;
        pts.push_back({ 0, HORIZON });
        for (int i = 0; x < SCREEN_W; i++) {
            x += 40 + noise(i * 11 + 5) * 50;
            bool middle = x > 420 && x < 860;
            pts.push_back({ fminf(x, (float)SCREEN_W), HORIZON - (middle ? 5 + noise(i * 13) * 20 : 20 + noise(i * 17) * 70) });
        }
        pts.push_back({ (float)SCREEN_W, HORIZON });
        for (size_t i = 0; i + 1 < pts.size(); i++) {
            ui_triangle(pts[i], pts[i + 1], { pts[i + 1].x, HORIZON }, hex(th.mountainFill));
            ui_triangle(pts[i], { pts[i + 1].x, HORIZON }, { pts[i].x, HORIZON }, hex(th.mountainFill));
        }
        for (size_t i = 0; i + 1 < pts.size(); i++) {
            DrawLineEx(pts[i], pts[i + 1], 5, hex(th.mountainEdge, 0.2f));
            DrawLineEx(pts[i], pts[i + 1], 1.5f, hex(th.mountainEdge));
        }
    }

    // The floor and its grid, the lines rushing towards you
    DrawRectangleGradientV(0, (int)HORIZON, SCREEN_W, 100, shade(hex(th.floor), 1.6f), hex(th.floor));
    DrawRectangle(0, (int)HORIZON + 100, SCREEN_W, SCREEN_H - (int)HORIZON - 100, hex(th.floor));
    Color grid = hex(th.grid, 0.4f);
    for (int i = -24; i <= 24; i++) DrawLineEx({ 640.0f + i * 34, HORIZON }, { 640.0f + i * 170, (float)SCREEN_H }, 1, grid);
    float phase = fmodf(t * 0.6f, 1.0f);
    for (int k = 0; k < 14; k++) {
        float n = k + phase;
        float y = HORIZON + 6 * (powf(1.33f, n) - 1) / 0.33f;
        if (y > SCREEN_H) break;
        DrawLineEx({ 0, y }, { (float)SCREEN_W, y }, 1, grid);
    }
    DrawLineEx({ 0, HORIZON }, { (float)SCREEN_W, HORIZON }, 7, hex(th.horizon, 0.25f));
    DrawLineEx({ 0, HORIZON }, { (float)SCREEN_W, HORIZON }, 2.5f, hex(th.horizon));
}

// ---------------------------------------------------------------------------
// Cubes, plants and the tree
// ---------------------------------------------------------------------------

// A cube seen from the corner: base is its bottom-middle on screen, s its width.
static void cube(Vector2 base, float s, Color col)
{
    float h = s * 0.5f, z = s * 0.62f;
    Vector2 top = { base.x, base.y - z - h }, r = { base.x + s / 2, base.y - z - h / 2 }, mid = { base.x, base.y - z },
            l = { base.x - s / 2, base.y - z - h / 2 };
    Vector2 lb = { base.x - s / 2, base.y - h / 2 }, rb = { base.x + s / 2, base.y - h / 2 };
    ui_triangle(l, mid, base, shade(col, 0.62f));
    ui_triangle(l, base, lb, shade(col, 0.62f));
    ui_triangle(mid, r, rb, shade(col, 0.8f));
    ui_triangle(mid, rb, base, shade(col, 0.8f));
    ui_triangle(top, r, mid, col);
    ui_triangle(top, mid, l, col);
}

static void glow(Vector2 at, float radius, Color col, float strength)
{
    for (int i = 3; i >= 1; i--) DrawCircleV(at, radius * (0.6f + 0.4f * i), Fade(col, strength * 0.12f));
}

// How tall a plant is (for the tags above it).
static float plant_height(int stage)
{
    static const int cubes[] = { 0, 2, 4, 6, 8 };
    return cubes[stage] * 7.5f + 14;
}

// A plant of cubes: a stem, leaves every other level, fruit when it's mastered.
// Bit rot: cubes go grey, some flicker out, and 0s and 1s float off it.
static void draw_plant(Vector2 base, int stage, bool rot, const GardenTheme &th, float t, int seed)
{
    const float cs = 12;
    if (stage == 0) {   // a seed: one small glowing cube
        glow({ base.x, base.y - 8 }, 10, COL_AMBER, 0.6f + 0.3f * sinf(t * 2 + seed));
        cube(base, 10, COL_AMBER);
        return;
    }
    static const int levels[] = { 0, 2, 4, 6, 8 };
    int h = levels[stage];
    Color stem = rot ? Color{ 110, 102, 90, 255 } : hex(th.stem), leaf = rot ? Color{ 122, 106, 80, 255 } : hex(th.leaf);
    for (int j = 0; j < h; j++) {
        // with rot, some cubes are missing, and a few flicker in and out
        if (rot && noise(seed * 31 + j) < 0.3f) continue;
        if (rot && noise(seed * 37 + j) < 0.2f && sinf(t * 9 + j) > 0.4f) continue;
        Vector2 at = { base.x, base.y - j * cs * 0.62f };
        if (!rot && j == h - 1) glow({ at.x, at.y - 10 }, 12, stem, 0.4f);
        cube(at, cs, stem);
        if (j % 2 == 1 && j < h - 1) {
            for (int side = -1; side <= 1; side += 2) {
                if (rot && noise(seed * 41 + j * 3 + side) < 0.55f) continue;
                cube({ at.x + side * cs * 0.9f, at.y + cs * 0.25f + (rot ? 5 : 0) }, cs * 0.85f, leaf);
            }
        }
    }
    if (stage == 4 && !rot) {   // in fruit: three glowing cubes on top
        Vector2 fruit[3] = { { -9, 0 }, { 9, 0 }, { 0, -8 } };
        float pulse = 0.7f + 0.3f * sinf(t * 3 + seed);
        for (int i = 0; i < 3; i++) {
            Vector2 at = { base.x + fruit[i].x, base.y - h * cs * 0.62f - 6 + fruit[i].y };
            glow({ at.x, at.y - 8 }, 12, hex(th.fruit), pulse);
            cube(at, 10, hex(th.fruit));
        }
    }
    if (rot) {   // 0s and 1s floating off
        for (int i = 0; i < 5; i++) {
            float k = fmodf(t * 0.5f + noise(seed * 7 + i), 1.0f);
            float x = base.x - 26 + i * 13, y = base.y - h * 7 - 10 - k * 46;
            const char *bit = noise(seed * 13 + i) < 0.5f ? "0" : "1";
            ui_text(bit, x, y, 11, Fade(COL_CORAL, (1 - k) * 0.9f), FontKind::MonoBold);
        }
    }
}

// The tree, main(): a trunk and a round canopy of cubes.
static void draw_tree(Vector2 base, const GardenTheme &th, float t)
{
    glow({ base.x, base.y - 90 }, 90, hex(th.canopy), 0.5f + 0.15f * sinf(t * 1.5f));
    for (int j = 0; j < 5; j++) cube({ base.x, base.y - j * 12.0f }, 20, hex(th.trunk));
    struct Cell { int lvl, q, w; };
    static std::vector<Cell> cells;
    if (cells.empty()) {
        static const int radius[] = { 3, 4, 4, 3, 2 };
        int n = 0;
        for (int lvl = 0; lvl < 5; lvl++)
            for (int q = -radius[lvl]; q <= radius[lvl]; q++)
                for (int w = -radius[lvl]; w <= radius[lvl]; w++)
                    if (q * q + w * w <= radius[lvl] * radius[lvl] + 1 && noise(++n * 19) > 0.2f) cells.push_back({ lvl, q, w });
        std::sort(cells.begin(), cells.end(), [](const Cell &a, const Cell &b) {
            return a.lvl != b.lvl ? a.lvl < b.lvl : a.q + a.w < b.q + b.w;
        });
    }
    for (size_t i = 0; i < cells.size(); i++) {
        const Cell &c = cells[i];
        bool fruit = noise((int)i * 23 + 3) < 0.14f;
        Color col = fruit ? hex(th.treeFruit) : hex(th.canopy);
        if (fruit) col = mixc(col, WHITE, 0.25f * (0.5f + 0.5f * sinf(t * 3 + i)));
        cube({ base.x + (c.q - c.w) * 8.0f, base.y - 62 - c.lvl * 11.0f + (c.q + c.w) * 4.0f }, 15, col);
    }
    const char *label = "main()";
    ui_text(label, base.x - ui_text_width(label, 12, FontKind::MonoBold) / 2, base.y - 152, 12, hex(th.canopy), FontKind::MonoBold);
}

// ---------------------------------------------------------------------------
// The slab and its cells
// ---------------------------------------------------------------------------

// Cell k (skipping the middle) in the grid, and its centre on screen.
static void plot_cell(int k, int &gx, int &gy)
{
    int cell = k < (GRID * GRID) / 2 ? k : k + 1;
    gx = cell % GRID;
    gy = cell / GRID;
}

static Vector2 cell_centre(int gx, int gy)
{
    float half = (GRID - 1) / 2.0f;
    return { CENTRE.x + (gx - gy) * TILE_W / 2, CENTRE.y + (gx + gy - 2 * half) * TILE_H / 2 };
}

static void diamond(Vector2 c, float w, float h, Color col)
{
    Vector2 top = { c.x, c.y - h / 2 }, right = { c.x + w / 2, c.y }, bottom = { c.x, c.y + h / 2 }, left = { c.x - w / 2, c.y };
    ui_triangle(top, right, bottom, col);
    ui_triangle(top, bottom, left, col);
}

static void diamond_lines(Vector2 c, float w, float h, float thick, Color col)
{
    Vector2 p[4] = { { c.x, c.y - h / 2 }, { c.x + w / 2, c.y }, { c.x, c.y + h / 2 }, { c.x - w / 2, c.y } };
    for (int i = 0; i < 4; i++) DrawLineEx(p[i], p[(i + 1) % 4], thick, col);
}

static Color track_colour(const std::string &track)
{
    if (track.find("basics") != std::string::npos) return COL_BLUE;
    if (track.find("Pointer") != std::string::npos) return COL_PURPLE;
    if (track.find("Linked") != std::string::npos) return COL_CORAL;
    return COL_AMBER;
}

struct GardenHover { int plot = -1; bool tree = false; };

// The floating slab, its cells, plants and the tree. Returns what the mouse is
// over (when mouse isn't null). Also used, scaled down, for the room's window.
static GardenHover draw_slab(const App &app, const GardenTheme &th, float t, const Vector2 *mouse, int &ripe, int &rotting)
{
    GardenHover hover;
    std::string today = today_string();
    int count = (int)app.topics.size() < 24 ? (int)app.topics.size() : 24;
    float half = 2.85f;
    Vector2 top = { CENTRE.x, CENTRE.y - half * TILE_H }, right = { CENTRE.x + half * TILE_W, CENTRE.y },
            bot = { CENTRE.x, CENTRE.y + half * TILE_H }, left = { CENTRE.x - half * TILE_W, CENTRE.y };
    Color edge = hex(th.edge);

    // A glow under the slab, then its two sides, with lights along them
    for (int i = 4; i >= 1; i--) DrawEllipse((int)CENTRE.x, (int)(CENTRE.y + SLAB_DEPTH + 30), half * TILE_W * (0.7f + 0.1f * i), half * TILE_H * (0.6f + 0.1f * i), hex(th.edge, 0.04f));
    Vector2 dl = { left.x, left.y + SLAB_DEPTH }, db = { bot.x, bot.y + SLAB_DEPTH }, dr = { right.x, right.y + SLAB_DEPTH };
    ui_triangle(left, bot, db, shade(hex(th.side), 0.8f));
    ui_triangle(left, db, dl, shade(hex(th.side), 0.8f));
    ui_triangle(bot, right, dr, hex(th.side));
    ui_triangle(bot, dr, db, hex(th.side));
    for (int k = 1; k < 9; k++) {
        float u = k / 9.0f;
        for (int s = 0; s < 2; s++) {
            Vector2 a = s == 0 ? left : bot, b = s == 0 ? bot : right;
            Vector2 p = { a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u };
            DrawLineEx({ p.x, p.y + 4 }, { p.x, p.y + SLAB_DEPTH - 4 }, 1, hex(th.edge, 0.35f));
            float blink = sinf(t * 2 + k * 1.7f + s * 3);
            if (blink > 0) DrawCircleV({ p.x, p.y + SLAB_DEPTH * 0.5f }, 2, hex(th.edge, blink));
        }
    }
    DrawLineEx(dl, db, 1, hex(th.edge, 0.6f));
    DrawLineEx(db, dr, 1, hex(th.edge, 0.6f));
    DrawLineEx(bot, db, 1, hex(th.edge, 0.6f));
    // The top, edged in neon
    ui_triangle(top, right, bot, hex(th.slab));
    ui_triangle(top, bot, left, hex(th.slab));
    diamond_lines(CENTRE, half * TILE_W * 2, half * TILE_H * 2, 6, hex(th.edge, 0.25f));
    diamond_lines(CENTRE, half * TILE_W * 2, half * TILE_H * 2, 2, edge);

    // The cells back to front, so nearer plants draw over farther ones
    struct Tag { Vector2 at; int value; bool rot; int k; float h; };
    std::vector<Tag> tags;
    Color plotFill = shade(hex(th.slab), 1.35f);
    for (int d = 0; d <= 2 * (GRID - 1); d++) {
        for (int gx = 0; gx < GRID; gx++) {
            int gy = d - gx;
            if (gy < 0 || gy >= GRID) continue;
            Vector2 c = cell_centre(gx, gy);
            bool over = mouse && (CheckCollisionPointTriangle(*mouse, { c.x, c.y - TILE_H / 2 }, { c.x + TILE_W / 2, c.y }, { c.x, c.y + TILE_H / 2 }) ||
                                  CheckCollisionPointTriangle(*mouse, { c.x, c.y - TILE_H / 2 }, { c.x, c.y + TILE_H / 2 }, { c.x - TILE_W / 2, c.y }));
            if (gx == GRID / 2 && gy == GRID / 2) {   // the middle: the tree
                diamond(c, TILE_W - 18, TILE_H - 10, plotFill);
                diamond_lines(c, TILE_W - 18, TILE_H - 10, 1.5f, hex(th.canopy, 0.8f));
                if (mouse && (over || CheckCollisionPointRec(*mouse, { c.x - 70, c.y - 160, 140, 150 }))) hover.tree = true;
                draw_tree(c, th, t);
                continue;
            }
            int k = gy * GRID + gx;
            k = k > (GRID * GRID) / 2 ? k - 1 : k;
            if (k >= count) {   // a spare cell
                diamond(c, TILE_W - 18, TILE_H - 10, plotFill);
                diamond_lines(c, TILE_W - 18, TILE_H - 10, 1, hex(th.edge, 0.15f));
                continue;
            }
            const Topic &tp = app.topics[k];
            int stage = crop_stage(app.progress, tp.id);
            bool rot = crop_wilting(app.progress, tp.id, today);
            int value = crop_harvest_value(app.progress, tp.id, today);
            if (value > 0) ripe++;
            if (rot) rotting++;
            if (over) hover.plot = k;
            Color col = track_colour(tp.track);
            Color fill = rot ? Color{ 58, 10, 32, 255 } : plotFill;
            if (over) fill = shade(fill, 1.5f);
            diamond(c, TILE_W - 18, TILE_H - 10, fill);
            if (stage > 0 && !rot) diamond(c, TILE_W - 18, TILE_H - 10, Fade(col, 0.06f));
            diamond_lines(c, TILE_W - 18, TILE_H - 10, stage > 0 ? 1.5f : 1, rot ? COL_CORAL : Fade(col, stage > 0 ? 0.95f : 0.3f));
            char addr[16];
            snprintf(addr, sizeof addr, "0x%04X", 0x1000 + k * 0x40);
            if (mouse) ui_text(addr, c.x - ui_text_width(addr, 9, FontKind::Mono) / 2, c.y + TILE_H / 2 - 22, 9, Fade(COL_MUTED, 0.8f), FontKind::Mono);
            draw_plant({ c.x, c.y + 4 }, stage, rot, th, t, k);
            if (rot || value > 0) tags.push_back({ c, value, rot, k, plant_height(stage) });
        }
    }
    // Tags and bytes on top of everything (not in the little window view)
    if (!mouse) tags.clear();
    for (const Tag &g : tags) {
        if (g.rot) {
            const char *tag = "bit rot! review";
            float tw = ui_text_width(tag, 11, FontKind::MonoBold) + 16;
            ui_panel({ g.at.x - tw / 2, g.at.y - g.h - 40, tw, 20 }, { 18, 6, 10, 235 }, COL_CORAL, 1);
            ui_text(tag, g.at.x - tw / 2 + 8, g.at.y - g.h - 37, 11, COL_CORAL, FontKind::MonoBold);
        } else {
            float hop = fabsf(sinf(t * 4 + g.k)) * 5;
            Vector2 at = { g.at.x + 30, g.at.y - g.h - 14 - hop };
            glow(at, 14, hex(th.coin), 1);
            DrawCircleV(at, 11, hex(th.coin));
            DrawCircleV(at, 8, shade(hex(th.coin), 0.8f));
            std::string n = std::to_string(g.value);
            ui_text(n, at.x - ui_text_width(n, 10, FontKind::MonoBold) / 2, at.y - 6, 10, COL_BG, FontKind::MonoBold);
        }
    }
    return hover;
}

static int theme_today()
{
    return garden_theme_for(today_string());
}

// The view out of Mal's window: today's garden, small.
void draw_garden_view(const App &app, Rectangle area, float t)
{
    const GardenTheme &th = THEMES[theme_today()];
    float scale = fmaxf(area.width / 760.0f, area.height / 720.0f);   // the whole island, filling the window
    rlPushMatrix();
    rlTranslatef(area.x + area.width / 2 - 630 * scale, area.y + area.height / 2 - 390 * scale, 0);
    rlScalef(scale, scale, 1);
    draw_backdrop(th, t);
    int ripe = 0, rotting = 0;
    draw_slab(app, th, t, nullptr, ripe, rotting);
    rlPopMatrix();
}

// ---------------------------------------------------------------------------
// The screen
// ---------------------------------------------------------------------------

void open_farm(App &app)
{
    if (app.screen != Screen::Farm) app.returnTo = app.screen;
    app.screen = Screen::Farm;
    treeOpen = false;
    previewTheme = -1;
}

static const char *GC_C[] = {
    "for (int i = 0; i < 20; i++) {",
    "    gc_visit(&cells[i]);",
    "    if (rotting(i))",
    "        mark(i);     // you review it",
    "    if (ripe(i))",
    "        collect(i);",
    "}",
};

static void draw_bot(Vector2 at, Color accent, float t)
{
    float bob = sinf(t * 5) * 3;
    Vector2 c = { at.x, at.y + bob };
    DrawEllipse((int)at.x, (int)(at.y + 90), 22, 6, Fade(BLACK, 0.3f));
    DrawTriangle({ c.x - 8, c.y + 10 }, { c.x - 30, c.y + 88 }, { c.x + 30, c.y + 88 }, Fade(accent, 0.08f));
    DrawTriangle({ c.x - 8, c.y + 10 }, { c.x + 30, c.y + 88 }, { c.x + 8, c.y + 10 }, Fade(accent, 0.08f));
    DrawLineEx({ c.x - 36, c.y - 16 }, { c.x + 36, c.y - 16 }, 3, accent);
    for (int side = -1; side <= 1; side += 2) {
        float spin = fabsf(sinf(t * 40 + side));
        DrawEllipse((int)(c.x + side * 36), (int)(c.y - 19), 20 * spin + 4, 3, Fade(accent, 0.7f));
    }
    glow(c, 26, accent, 0.6f);
    DrawRectangleRounded({ c.x - 28, c.y - 12, 56, 24 }, 0.5f, 6, { 24, 18, 52, 255 });
    DrawRectangleRoundedLinesEx({ c.x - 28, c.y - 12, 56, 24 }, 0.5f, 6, 1.5f, accent);
    ui_text("gc", c.x - ui_text_width("gc", 13, FontKind::MonoBold) / 2, c.y - 7, 13, accent, FontKind::MonoBold);
}

void screen_farm(App &app)
{
    float t = (float)GetTime(), dt = GetFrameTime();
    std::string today = today_string();
    int count = (int)app.topics.size() < 24 ? (int)app.topics.size() : 24;
    int themeIndex = previewTheme >= 0 ? previewTheme : theme_today();
    const GardenTheme &th = THEMES[themeIndex];
    Color accent = hex(th.accent);
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (treeOpen) treeOpen = false;
        else { open_room(app); return; }
    }
    if (!treeOpen && IsKeyPressed(KEY_RIGHT)) previewTheme = (themeIndex + 1) % GARDEN_THEMES;
    if (!treeOpen && IsKeyPressed(KEY_LEFT)) previewTheme = (themeIndex + GARDEN_THEMES - 1) % GARDEN_THEMES;

    draw_backdrop(th, t);
    Vector2 mouse = GetMousePosition();
    int ripe = 0, rotting = 0;
    GardenHover hover = draw_slab(app, th, t, treeOpen ? nullptr : &mouse, ripe, rotting);

    // ----- The gc bot: visits the cells in a for loop -----
    bool bot = farm_has_drone(app.progress);
    if (bot && count > 0 && !treeOpen) {
        float before = botT;
        botT += dt / 1.1f;
        if (botT >= count) { botT = 0; botDone = 0; }
        int i = (int)botT;
        float u = botT - i;
        if ((int)before != i || before == 0) {   // arrived at cell i: does it need anything?
            botDone = i + 1;
            const Topic &tp = app.topics[i];
            int got = crop_harvest(app.progress, tp.id, today);
            int gx, gy;
            plot_cell(i, gx, gy);
            Vector2 c = cell_centre(gx, gy);
            if (got > 0) {
                save_progress(app);
                fx_float_text("+" + std::to_string(got) + " bytes", { c.x, c.y - 70 }, 18, hex(th.coin));
                fx_burst({ c.x, c.y - 30 }, 14, hex(th.coin), accent);
                quest_event(app, "harvest");
                botLine = 6;
            } else if (crop_wilting(app.progress, tp.id, today)) {
                botLine = 4;
            } else {
                botLine = 2;
            }
        }
        int gx, gy, nx, ny;
        plot_cell(i, gx, gy);
        plot_cell((i + 1) % count, nx, ny);
        Vector2 a = cell_centre(gx, gy), b = cell_centre(nx, ny);
        float e = u * u * (3 - 2 * u);
        draw_bot({ a.x + (b.x - a.x) * e, a.y + (b.y - a.y) * e - 120 }, accent, t);
        if (u > 0.5f) botLine = 2;
    }

    // ----- Mal -----
    mal_set_base(app.mal, rotting > 0 ? MalMood::Idle : MalMood::Happy);
    mal_draw_with_pet(app.mal, { 150, 650 }, 90);

    // ----- The header: name, today's look, bytes -----
    ui_panel({ 20, 16, 640, 58 }, Fade({ 13, 17, 23, 255 }, 0.9f), Fade(accent, 0.6f), 1);
    ui_text("BIT GARDEN", 40, 32, 22, COL_TEXT, FontKind::MonoBold);
    static const char *days[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
    std::string look = std::string(days[themeIndex]) + ": " + garden_theme_name(themeIndex) + (previewTheme >= 0 && themeIndex != theme_today() ? " (preview)" : "");
    ui_text(look, 200, 38, 13, Fade(accent, 0.85f), FontKind::Mono);
    std::string info = std::to_string(app.progress.bytes) + " bytes | " + std::to_string(ripe) + " ripe | " + std::to_string(rotting) + " bit rot";
    ui_text(info, 640 - 16 - ui_text_width(info, 13, FontKind::MonoBold), 38, 13, accent, FontKind::MonoBold);
    if (ui_button("Back to the room", { SCREEN_W - 40 - 190, 18, 190, 44 }, ButtonStyle::Primary, COL_AMBER)) { open_room(app); return; }

    // ----- gc.c, or buying the bot -----
    Rectangle panel = { 930, 432, 320, 222 };
    ui_panel(panel, { 13, 17, 23, 238 }, Fade(accent, 0.5f), 1);
    ui_text("gc.c", panel.x + 18, panel.y + 14, 14, accent, FontKind::MonoBold);
    for (int i = 0; i < 7; i++) {
        bool on = bot && i + 1 == botLine;
        if (on) DrawRectangle((int)panel.x + 8, (int)(panel.y + 38 + i * 21), (int)panel.width - 16, 20, Fade(accent, 0.16f));
        ui_text(GC_C[i], panel.x + 18, panel.y + 41 + i * 21, 12, on ? COL_TEXT : (bot ? COL_TEXT_SOFT : COL_MUTED), FontKind::Mono);
    }
    if (bot) {
        ui_text("> running  |  cell " + std::to_string(botDone) + " of " + std::to_string(count), panel.x + 18, panel.y + panel.height - 26, 12,
                COL_GREEN, FontKind::Mono);
    } else {
        bool afford = app.progress.bytes >= DRONE_PRICE;
        if (ui_button("Buy the gc bot  " + std::to_string(DRONE_PRICE) + " bytes", { panel.x + 18, panel.y + panel.height - 50, panel.width - 36, 38 },
                      afford ? ButtonStyle::Primary : ButtonStyle::Disabled, accent)) {
            std::string why;
            if (farm_buy_drone(app.progress, why)) {
                save_progress(app);
                sfx_play(Sfx::LevelUp);
                fx_burst({ panel.x + panel.width / 2, panel.y }, 50, accent, COL_AMBER);
                botT = 0;
            }
        }
    }

    // ----- What's under the mouse, and clicking it -----
    if (hover.plot >= 0) {
        const Topic &tp = app.topics[hover.plot];
        int stage = crop_stage(app.progress, tp.id);
        bool rot = crop_wilting(app.progress, tp.id, today);
        int value = crop_harvest_value(app.progress, tp.id, today);
        int left = crop_days_left(app.progress, tp.id, today);
        static const char *stages[] = { "a seed", "a sprout", "growing", "nearly there", "in fruit" };
        std::string l1 = tp.title;
        std::string l2 = std::string(stages[stage]) + "  |  " + mastery_name(topic_mastery(app.progress, tp.id));
        std::string l3 = value > 0 ? "Click to harvest: +" + std::to_string(value) + " bytes"
                       : rot ? "Bit rot! Click to quiz it and fix it."
                       : stage == 0 ? "Click to plant it: start the lesson."
                       : "Fresh for " + std::to_string(left) + (left == 1 ? " more day" : " more days") + ". Click to quiz it.";
        float w = fmaxf(ui_text_width(l1, 15, FontKind::UiBold), fmaxf(ui_text_width(l2, 12, FontKind::Mono), ui_text_width(l3, 13))) + 24;
        float x = fminf(mouse.x + 18, SCREEN_W - w - 10), y = mouse.y + 16;
        ui_panel({ x, y, w, 74 }, Fade(COL_BG, 0.95f), rot ? COL_CORAL : value > 0 ? hex(th.coin) : accent, 1);
        ui_text(l1, x + 12, y + 8, 15, COL_TEXT, FontKind::UiBold);
        ui_text(l2, x + 12, y + 30, 12, COL_MUTED, FontKind::Mono);
        ui_text(l3, x + 12, y + 50, 13, rot ? COL_CORAL : value > 0 ? hex(th.coin) : COL_TEXT_SOFT);
        ui_want_hand();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int gx, gy;
            plot_cell(hover.plot, gx, gy);
            Vector2 c = cell_centre(gx, gy);
            if (value > 0) {
                int got = crop_harvest(app.progress, tp.id, today);
                save_progress(app);
                sfx_play(Sfx::Combo);
                fx_burst({ c.x, c.y - 30 }, 24, hex(th.coin), accent);
                fx_float_text("+" + std::to_string(got) + " bytes", { c.x, c.y - 70 }, 20, hex(th.coin));
                quest_event(app, "harvest");
            } else {
                app.selected = hover.plot;   // the topic map's selection: the lesson and quiz open for it
                app.inCampaign = false;
                if (stage == 0) open_lesson(app);
                else open_quiz(app);
                return;
            }
        }
    } else if (hover.tree) {
        ui_want_hand();
        const char *tip = "main(): your code tree. Click to look closer";
        float w = ui_text_width(tip, 14, FontKind::UiBold) + 20;
        float x = fminf(mouse.x + 16, SCREEN_W - w - 10);
        ui_panel({ x, mouse.y + 14, w, 28 }, Fade(COL_BG, 0.92f), accent, 1);
        ui_text(tip, x + 10, mouse.y + 20, 14, COL_TEXT, FontKind::UiBold);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { treeOpen = true; sfx_play(Sfx::Click); }
    }

    DrawRectangle(0, SCREEN_H - 36, SCREEN_W, 36, Fade({ 8, 10, 14, 255 }, 0.85f));
    ui_text("review cells before bit rot  |  harvest ripe ones for bytes  |  <- -> see the other days  |  ESC back to the room", 40,
            SCREEN_H - 24, 12, Fade(COL_TEXT_SOFT, 0.9f), FontKind::Mono);

    if (treeOpen) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.6f));
        Rectangle p = { 140, 50, SCREEN_W - 280.0f, SCREEN_H - 100.0f };
        ui_panel(p, { 12, 22, 40, 255 }, COL_GREEN, 2);
        ui_text("YOUR CODE TREE", p.x + 28, p.y + 22, 13, COL_GREEN, FontKind::MonoBold);
        ui_text("A branch for each region, a leaf for each topic. Leaves grow as you learn; gold blossoms are Mastered,"
                " and stars mark beaten bosses.", p.x + 28, p.y + 42, 14, COL_TEXT_SOFT);
        draw_code_tree(app, { p.x + 40, p.y + 80, p.width - 80, p.height - 130 }, t, true);
        if (ui_button("Close", { p.x + p.width - 28 - 120, p.y + p.height - 28 - 44, 120, 44 }, ButtonStyle::Outline, COL_TEXT))
            treeOpen = false;
    }
}
