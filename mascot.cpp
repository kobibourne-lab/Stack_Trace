// mascot.cpp - drawing and animating Mal.

#include "mascot.h"
#include "theme.h"
#include "ui.h"
#include "rlgl.h"
#include <cmath>

void mal_react(Mal &m, MalMood mood, float seconds)
{
    m.mood = mood;
    m.moodLeft = seconds;
    switch (mood) {
        case MalMood::Happy:   m.hopVel = -330; m.squashVel = 5; break;   // jump, stretching
        case MalMood::Cheer:   m.hopVel = -420; m.squashVel = 6; break;
        case MalMood::Sad:     m.squash = 0.78f; break;                    // flattened
        case MalMood::Worried: m.squashVel = -3; break;
        default: break;
    }
}

void mal_set_base(Mal &m, MalMood mood)
{
    m.base = mood;
    // A reaction that's still playing, or being freed, isn't interrupted
    if (m.moodLeft <= 0 && m.mood != MalMood::Freed) m.mood = mood;
}

void mal_update(Mal &m, float dt)
{
    m.clock += dt;

    if (m.moodLeft > 0) {
        m.moodLeft -= dt;
        if (m.moodLeft <= 0) { m.moodLeft = 0; m.mood = m.base; }
    }

    // Squash and stretch: a spring pulling back to 1
    m.squashVel += (1.0f - m.squash) * 260 * dt;
    m.squashVel *= 1.0f - 9 * dt;
    m.squash += m.squashVel * dt;

    // Hopping: gravity, and a squash on landing
    if (m.hop < 0 || m.hopVel != 0) {
        m.hopVel += 1500 * dt;
        m.hop += m.hopVel * dt;
        if (m.hop >= 0) {
            m.hop = 0;
            m.squashVel -= m.hopVel * 0.012f;   // land: squash in proportion to the fall
            m.hopVel = 0;
            if (m.mood == MalMood::Cheer) m.hopVel = -360;   // cheering keeps bouncing
        }
    }

    // Blinking every few seconds (not while asleep, happy or crumbled)
    if (m.blinkLeft > 0) m.blinkLeft -= dt;
    m.blinkIn -= dt;
    if (m.blinkIn <= 0) {
        m.blinkLeft = 0.13f;
        m.blinkIn = 2.0f + fmodf(m.clock * 7.31f, 2.5f);   // a varied gap, no rand needed
    }

    // Crumbling into cells, and building back up again
    float target = (m.mood == MalMood::Freed) ? 1.0f : 0.0f;
    float speed = (target > m.crumble) ? 0.7f : 1.4f;
    if (m.crumble < target) m.crumble = fminf(target, m.crumble + speed * dt);
    if (m.crumble > target) m.crumble = fmaxf(target, m.crumble - speed * dt);
}

// A number from 0 to 1 that's always the same for the same cell, so the pile
// lands the same way every time.
static float hash01(int n)
{
    unsigned int x = (unsigned int)n * 2654435761u;
    x ^= x >> 13; x *= 1274126177u; x ^= x >> 16;
    return (x & 0xffff) / 65535.0f;
}

// Mal's colour from the wardrobe, then changed a little by mood.
static Color skin_color(const std::string &skin)
{
    if (skin == "amber")  return COL_AMBER;
    if (skin == "purple") return COL_PURPLE;
    if (skin == "blue")   return COL_BLUE;
    if (skin == "slayer")  return { 255, 205, 90, 255 };    // boss slayer gold
    if (skin == "diamond") return { 175, 232, 255, 255 };
    if (skin == "tux")     return { 236, 239, 243, 255 };   // a white shirt; the jacket is drawn on
    if (skin == "offbyone") return COL_CORAL;
    if (skin == "shallow")  return COL_PURPLE;
    return COL_GREEN;   // classic, glitch and leaker
}

static Color blend(Color a, Color b, float t)
{
    return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
             (unsigned char)(a.b + (b.b - a.b) * t), 255 };
}

static Color body_color(const Mal &m)
{
    Color base = skin_color(m.skin);
    if (m.skin == "dangling")   // half freed: a faded blue that flickers
        base = blend(COL_BLUE, COL_BG, 0.5f + 0.12f * sinf(m.clock * 9) + (fmodf(m.clock, 2.3f) < 0.08f ? 0.25f : 0));
    switch (m.mood) {
        case MalMood::Worried: return m.skin == "amber" ? Color{ 255, 140, 60, 255 } : blend(base, COL_AMBER, 0.7f);
        case MalMood::Sad:     return blend(base, { 120, 130, 140, 255 }, 0.3f);   // a duller shade
        case MalMood::Freed:   return { 120, 128, 138, 255 };
        default:               return base;
    }
}

static Color darker(Color c, float f) { return { (unsigned char)(c.r * f), (unsigned char)(c.g * f), (unsigned char)(c.b * f), c.a }; }

static void draw_eye(const Mal &m, Vector2 c, float r, bool left)
{
    Color ink = COL_BG;
    switch (m.mood) {
        case MalMood::Happy:
        case MalMood::Cheer: {   // ^ shaped: eyes closed in a smile
            float t = r * 0.45f;
            DrawLineEx({ c.x - r, c.y + r * 0.35f }, { c.x, c.y - r * 0.55f }, t, ink);
            DrawLineEx({ c.x, c.y - r * 0.55f }, { c.x + r, c.y + r * 0.35f }, t, ink);
            return;
        }
        case MalMood::Sleep:     // closed: a gentle downward curve
            DrawRing(c, r * 0.75f, r * 1.05f, 20, 160, 12, ink);
            return;
        case MalMood::Freed: {   // X
            float t = r * 0.4f;
            DrawLineEx({ c.x - r * 0.8f, c.y - r * 0.8f }, { c.x + r * 0.8f, c.y + r * 0.8f }, t, ink);
            DrawLineEx({ c.x - r * 0.8f, c.y + r * 0.8f }, { c.x + r * 0.8f, c.y - r * 0.8f }, t, ink);
            return;
        }
        default: break;
    }

    float rr = (m.mood == MalMood::Worried) ? r * 1.25f : r;
    if (m.blinkLeft > 0) {        // blink: squeeze to a line
        DrawEllipse((int)c.x, (int)c.y, rr, rr * 0.15f, ink);
        return;
    }
    DrawCircleV(c, rr, ink);
    if (m.mood == MalMood::Worried) {
        DrawCircleV(c, rr * 0.7f, COL_TEXT);          // wide whites, small pupils
        DrawCircleV({ c.x, c.y + rr * 0.1f }, rr * 0.35f, ink);
    } else {
        // A highlight, drifting a little as Mal looks around
        float look = (m.mood == MalMood::Idle) ? sinf(m.clock * 0.7f) * r * 0.25f : 0;
        DrawCircleV({ c.x + look + r * 0.25f, c.y - r * 0.3f }, r * 0.32f, COL_TEXT);
    }
    if (m.mood == MalMood::Sad) {  // droopy lids slanting down to the outside
        float dir = left ? -1.0f : 1.0f;
        DrawLineEx({ c.x - dir * r * 1.1f, c.y - r * 1.15f }, { c.x + dir * r * 1.2f, c.y - r * 0.55f }, r * 0.45f, ink);
    }
}

static void draw_mouth(const Mal &m, Vector2 c, float w, Color body)
{
    Color ink = COL_BG;
    switch (m.mood) {
        case MalMood::Happy:
        case MalMood::Cheer:
            DrawCircleSector(c, w * 0.55f, 0, 180, 16, ink);                 // wide open smile
            DrawCircleSector({ c.x, c.y + w * 0.28f }, w * 0.26f, 0, 180, 12, COL_CORAL);   // tongue
            break;
        case MalMood::Sad:
            DrawRing({ c.x, c.y + w * 0.45f }, w * 0.32f, w * 0.46f, 200, 340, 12, ink);   // frown
            break;
        case MalMood::Worried:
            DrawEllipse((int)c.x, (int)(c.y + w * 0.1f), w * 0.18f, w * 0.24f, ink);
            break;
        case MalMood::Sleep:
            DrawLineEx({ c.x - w * 0.15f, c.y + w * 0.1f }, { c.x + w * 0.15f, c.y + w * 0.1f }, w * 0.12f, ink);
            break;
        case MalMood::Freed: {   // a wobbly line
            float t = w * 0.1f;
            for (int i = 0; i < 4; i++) {
                float x0 = c.x - w * 0.4f + i * w * 0.2f;
                DrawLineEx({ x0, c.y + (i % 2 ? w * 0.1f : -w * 0.05f) },
                           { x0 + w * 0.2f, c.y + (i % 2 ? -w * 0.05f : w * 0.1f) }, t, ink);
            }
            break;
        }
        default:   // Idle: a small smile
            DrawRing(c, w * 0.26f, w * 0.38f, 20, 160, 12, ink);
            break;
    }
    (void)body;
}

// The wardrobe: hats and accessories, drawn on the body rectangle b so they
// squash and stretch with Mal.
// ---------------------------------------------------------------------------
// The new hair, boss skins, glitch skin and pets
// ---------------------------------------------------------------------------

static Color mixc(Color a, Color b, float t)
{
    return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
             (unsigned char)(a.b + (b.b - a.b) * t), a.a };
}

// The address on Mal's label: the skins change it.
static std::string mal_address(const Mal &m)
{
    if (m.skin == "offbyone") return "0x9001";                   // one byte off
    if (m.skin == "dangling") return "freed";
    if (m.skin == "glitch") {
        static const char *junk[] = { "0x9?0#", "0x9000", "0x#00?", "0x9000", "0xDEAD" };
        return junk[(int)(m.clock * 5) % 5];
    }
    return "0x9000";
}

static void brace(const char *ch, float x, float y, float fs, Color c)
{
    ui_text(ch, x, y, fs, c, FontKind::MonoBold);
    ui_text(ch, x + fs * 0.05f, y, fs, c, FontKind::MonoBold);   // thicker strokes
}

const Color AFRO = { 150, 105, 255, 255 };

// The afro: the big round part goes behind Mal...
static void draw_afro_back(Rectangle b, float size)
{
    Color back = darker(AFRO, 0.7f);
    Vector2 c = { b.x + b.width / 2, b.y + size * 0.06f };
    DrawCircleV(c, size * 0.54f, back);
    for (int i = 0; i < 12; i++) {
        float a = PI * 0.92f + (PI * 1.16f) * i / 11;
        Vector2 p = { c.x + cosf(a) * size * 0.47f, c.y + sinf(a) * size * 0.47f };
        float fs = size * 0.30f;
        brace(i % 2 ? "}" : "{", p.x - fs * 0.3f, p.y - fs * 0.5f, fs, AFRO);
    }
}

// ...and a fringe of curls comes down over the top of the head, so there's
// no big bare forehead.
static void draw_afro_front(Rectangle b, float size)
{
    Color light = mixc(AFRO, WHITE, 0.15f);
    float w = b.width;
    DrawRectangleRounded({ b.x - size * 0.03f, b.y - size * 0.1f, w + size * 0.06f, size * 0.19f }, 0.9f, 10, AFRO);
    for (int i = 0; i < 7; i++) {
        float t = i / 6.0f, fs = size * 0.27f;
        float x = b.x - size * 0.06f + t * (w + size * 0.02f);
        float y = b.y - size * 0.13f + sinf(t * PI) * -size * 0.05f;
        brace(i % 2 ? "}" : "{", x, y, fs, light);
    }
    // curls down the sides, over the corners of the head
    brace("{", b.x - size * 0.1f, b.y + size * 0.02f, size * 0.30f, AFRO);
    brace("}", b.x + w - size * 0.08f, b.y + size * 0.02f, size * 0.30f, AFRO);
}

// One upside-down semicolon: the dot sits on the head, the comma's tail is a spike.
static void spike_semicolon(float cx, float bottom, float H, float r, Color c, float lean)
{
    DrawCircleV({ cx, bottom - r }, r, c);
    Vector2 head = { cx, bottom - r * 3.4f };
    DrawCircleV(head, r * 1.15f, c);
    Vector2 tip = { cx + lean, bottom - H };
    Vector2 l = { head.x - r * 1.1f, head.y + r * 0.2f }, rr = { head.x + r * 1.1f, head.y + r * 0.2f };
    DrawTriangle(l, rr, tip, c);
    DrawTriangle(rr, l, tip, c);
}

static void draw_mohawk(Rectangle b, float size)
{
    float cx = b.x + b.width / 2;
    const float hs[] = { 0.36f, 0.50f, 0.60f, 0.50f, 0.36f };
    for (int i = 0; i < 5; i++)
        spike_semicolon(cx + (i - 2) * size * 0.14f, b.y + size * 0.03f, size * hs[i], size * 0.042f,
                        mixc(COL_CORAL, COL_AMBER, hs[i] / 0.6f), (i - 2) * size * 0.05f);
}

static void dashed_rect(Rectangle r, float th, Color c)
{
    const float d = 7, gp = 5;
    for (float x = r.x; x < r.x + r.width; x += d + gp) {
        float e = fminf(x + d, r.x + r.width);
        DrawLineEx({ x, r.y }, { e, r.y }, th, c);
        DrawLineEx({ x, r.y + r.height }, { e, r.y + r.height }, th, c);
    }
    for (float y = r.y; y < r.y + r.height; y += d + gp) {
        float e = fminf(y + d, r.y + r.height);
        DrawLineEx({ r.x, y }, { r.x, e }, th, c);
        DrawLineEx({ r.x + r.width, y }, { r.x + r.width, e }, th, c);
    }
}

const Color GOO = { 140, 210, 90, 255 };


// A four-pointed sparkle.
static void star4(Vector2 c, float r, Color col)
{
    if (r < 0.5f) return;
    float t = r * 0.22f;
    DrawTriangle({ c.x, c.y - r }, { c.x - t, c.y }, { c.x + t, c.y }, col);
    DrawTriangle({ c.x - t, c.y }, { c.x, c.y + r }, { c.x + t, c.y }, col);
    DrawTriangle({ c.x - r, c.y }, { c.x, c.y + t }, { c.x, c.y - t }, col);
    DrawTriangle({ c.x, c.y - t }, { c.x, c.y + t }, { c.x + r, c.y }, col);
}

// Little cut diamonds circling Mal: the ones behind are drawn first, the ones
// in front last, so they really go round.
static void diamond_shards(const Mal &m, Rectangle b, float size, bool front)
{
    Vector2 c = { b.x + b.width / 2, b.y + b.height * 0.5f };
    for (int i = 0; i < 3; i++) {
        float a = m.clock * 1.4f + i * (2 * PI / 3);
        bool inFront = sinf(a) > 0;
        if (inFront != front) continue;
        float depth = 0.75f + 0.25f * sinf(a);
        Vector2 p = { c.x + cosf(a) * b.width * 0.9f, c.y + sinf(a) * b.height * 0.46f };   // a tilted orbit: in front low, behind high
        float r = size * 0.1f * depth;
        Color top = ColorFromHSV(fmodf(190 + i * 40 + m.clock * 40, 360), 0.25f, 1.0f), side = { 120, 200, 255, 255 };
        Vector2 L = { p.x - r, p.y }, R = { p.x + r, p.y }, T = { p.x, p.y - r * 0.8f }, B = { p.x, p.y + r * 1.3f };
        DrawTriangle(L, R, T, Fade(top, depth));
        DrawTriangle(L, B, R, Fade(side, depth));
        DrawTriangle({ p.x - r * 0.3f, p.y }, B, { p.x + r * 0.3f, p.y }, Fade(WHITE, 0.5f * depth));
    }
}

// The gem itself: a net of facets in icy shades, with a shine sweeping across.
static void diamond_facets(const Mal &m, Rectangle b, float size)
{
    const int cols = 4, rows = 5;
    float inset = b.width * 0.14f;
    Rectangle r = { b.x + inset * 0.5f, b.y + inset * 0.4f, b.width - inset, b.height - inset * 0.8f };
    Vector2 pts[rows + 1][cols + 1];
    for (int y = 0; y <= rows; y++)
        for (int x = 0; x <= cols; x++) {
            bool edge = x == 0 || y == 0 || x == cols || y == rows;
            float jx = edge ? 0 : (hash01(x * 31 + y * 7) - 0.5f) * r.width / cols * 0.6f;
            float jy = edge ? 0 : (hash01(x * 13 + y * 29) - 0.5f) * r.height / rows * 0.6f;
            pts[y][x] = { r.x + r.width * x / cols + jx, r.y + r.height * y / rows + jy };
        }
    float sweep = fmodf(m.clock * 0.5f, 1.6f) - 0.3f;   // where the shine is, 0..1 across
    Color shades[4] = { { 225, 248, 255, 255 }, { 150, 215, 255, 255 }, { 200, 190, 255, 255 }, { 120, 190, 245, 255 } };
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < cols; x++) {
            Vector2 a = pts[y][x], bb = pts[y][x + 1], c = pts[y + 1][x + 1], d = pts[y + 1][x];
            for (int half = 0; half < 2; half++) {
                Vector2 p0 = a, p1 = half ? c : d, p2 = half ? bb : c;
                if (half == 0) { p1 = d; p2 = c; }
                Color col = shades[(int)(hash01(x * 5 + y * 11 + half * 3) * 4) % 4];
                float cx = ((p0.x + p1.x + p2.x) / 3 - r.x) / r.width, cy = ((p0.y + p1.y + p2.y) / 3 - r.y) / r.height;
                float shine = fmaxf(0.0f, 1.0f - fabsf((cx + cy * 0.6f) / 1.6f * 1.6f - sweep) * 5);   // a diagonal band
                Color f = ColorLerp(col, WHITE, shine * 0.85f);
                DrawTriangle(p0, p1, p2, Fade(f, 0.55f + 0.35f * shine));
            }
            DrawLineEx(a, bb, 1, Fade(WHITE, 0.35f));
            DrawLineEx(a, d, 1, Fade(WHITE, 0.25f));
            DrawLineEx(a, c, 1, Fade(WHITE, 0.2f));
        }
    (void)size;
}

// A soft glow and slowly turning beams of light behind the diamond skin.
static void beam(Vector2 c, Vector2 p1, Vector2 p2, Color col)
{
    rlBegin(RL_TRIANGLES);   // bright at the centre, fading to nothing at the ends
    for (int wind = 0; wind < 2; wind++) {
        rlColor4ub(col.r, col.g, col.b, col.a); rlVertex2f(c.x, c.y);
        rlColor4ub(col.r, col.g, col.b, 0);
        if (wind) { rlVertex2f(p1.x, p1.y); rlVertex2f(p2.x, p2.y); }
        else { rlVertex2f(p2.x, p2.y); rlVertex2f(p1.x, p1.y); }
    }
    rlEnd();
}

static void diamond_aura(const Mal &m, Rectangle b, float size)
{
    Vector2 c = { b.x + b.width / 2, b.y + b.height * 0.5f };
    float pulse = 0.9f + 0.1f * sinf(m.clock * 2.5f);
    BeginBlendMode(BLEND_ADDITIVE);
    // The glow: a fan of triangles, bright in the middle and clear at the edge.
    // (Drawn by hand, because DrawCircleGradient takes different arguments in different raylib versions.)
    float gr = size * 0.95f * pulse;
    for (int i = 0; i < 32; i++) {
        float a0 = i * 2 * PI / 32, a1 = (i + 1) * 2 * PI / 32;
        beam(c, { c.x + cosf(a0) * gr, c.y + sinf(a0) * gr }, { c.x + cosf(a1) * gr, c.y + sinf(a1) * gr }, Fade({ 110, 200, 255, 255 }, 0.45f));
    }
    for (int i = 0; i < 10; i++) {
        float a = m.clock * 0.3f + i * PI / 5, spread = 0.07f;
        float len = size * (1.05f + 0.25f * sinf(m.clock * 1.7f + i * 1.3f));
        Vector2 p1 = { c.x + cosf(a - spread) * len, c.y + sinf(a - spread) * len };
        Vector2 p2 = { c.x + cosf(a + spread) * len, c.y + sinf(a + spread) * len };
        beam(c, p1, p2, Fade(ColorFromHSV(fmodf(180 + i * 18 + m.clock * 40, 360), 0.45f, 1.0f), 0.5f));
    }
    EndBlendMode();
}

// Skin extras drawn behind Mal's body.
static void draw_skin_back(const Mal &m, Rectangle b, Vector2 feet, float size, Color body)
{
    if (m.skin == "diamond") {
        diamond_aura(m, b, size);
        diamond_shards(m, b, size, false);
    } else if (m.skin == "leaker") {   // a puddle of leaked memory
        float wob = 1 + 0.04f * sinf(m.clock * 2);
        DrawEllipse((int)(feet.x + size * 0.08f), (int)(feet.y + 3), size * 0.62f * wob, size * 0.09f, Fade(GOO, 0.75f));
    } else if (m.skin == "shallow") {   // the copy: just an outline, sharing everything
        Rectangle g = { b.x + size * 0.42f, b.y - size * 0.12f, b.width, b.height };
        float bob = sinf(m.clock * 2.2f + 1) * size * 0.02f;
        g.y += bob;
        DrawRectangleRounded(g, 0.42f, 12, Fade(body, 0.22f));
        DrawRectangleRoundedLinesEx(g, 0.42f, 12, fmaxf(1.5f, size * 0.02f), body);
        float ey = g.y + g.height * 0.47f, ex = g.width * 0.2f, er = size * 0.05f;
        DrawCircleLinesV({ g.x + g.width / 2 - ex + size * 0.12f, ey }, er, body);
        DrawCircleLinesV({ g.x + g.width / 2 + ex + size * 0.12f, ey }, er, body);
        if (size >= 60) {   // both point at the same array
            Vector2 arr = { feet.x + size * 0.2f, b.y - size * 0.36f };
            for (int i = 0; i < 4; i++)
                DrawRectangleRec({ arr.x - size * 0.3f + i * size * 0.15f, arr.y, size * 0.13f, size * 0.13f }, COL_AMBER);
            DrawLineEx({ g.x + g.width / 2, g.y }, { arr.x + size * 0.15f, arr.y + size * 0.13f }, 2, Fade(COL_AMBER, 0.6f));
        }
    }
}

// Skin extras drawn on top of Mal, after the face.
static void draw_skin_front(const Mal &m, Rectangle b, Vector2 feet, float size, Color body)
{
    float w = b.width, h = b.height;
    float ex = w * 0.2f, ey = b.y + h * 0.47f, er = size * 0.07f;
    float line = fmaxf(2.0f, size * 0.025f);
    if (m.skin == "offbyone") {
        // a third eye, off by one place, and angry brows
        Vector2 e3 = { feet.x + ex * 2.1f, ey - er * 0.6f };
        DrawCircleV(e3, er * 0.95f, WHITE);
        DrawCircleV({ e3.x + er * 0.25f + sinf(m.clock * 1.3f) * er * 0.15f, e3.y }, er * 0.5f, COL_BG);
        DrawLineEx({ feet.x - ex - er, ey - er * 1.7f }, { feet.x - ex + er, ey - er * 1.1f }, line, COL_BG);
        DrawLineEx({ feet.x + ex + er, ey - er * 1.7f }, { feet.x + ex - er, ey - er * 1.1f }, line, COL_BG);
        if (size >= 60) ui_text("+1", b.x + w - size * 0.05f, b.y - size * 0.22f, size * 0.18f, COL_CORAL, FontKind::MonoBold);
    } else if (m.skin == "dangling") {
        dashed_rect({ b.x - size * 0.05f, b.y - size * 0.05f, w + size * 0.1f, h + size * 0.1f }, line, COL_CORAL);
        if (size >= 60) {   // a pointer still aimed at Mal, who isn't really there
            Vector2 a = { b.x - size * 0.38f, b.y + h * 0.55f }, t = { b.x - size * 0.1f, a.y };
            float bs = size * 0.18f;
            DrawRectangleRounded({ a.x - bs, a.y - bs / 2, bs, bs }, 0.3f, 6, COL_SURFACE);
            DrawRectangleRoundedLinesEx({ a.x - bs, a.y - bs / 2, bs, bs }, 0.3f, 6, line, COL_CORAL);
            ui_text("p", a.x - bs * 0.68f, a.y - bs * 0.42f, bs * 0.7f, COL_CORAL, FontKind::MonoBold);
            DrawLineEx(a, t, line, COL_CORAL);
            DrawTriangle({ t.x + size * 0.06f, t.y }, { t.x, t.y - size * 0.05f }, { t.x, t.y + size * 0.05f }, COL_CORAL);
        }
        for (int i = 0; i < 3; i++) {   // cells drifting away
            float k = fmodf(m.clock * 0.4f + i / 3.0f, 1.0f);
            float cs = size * (0.07f - i * 0.015f);
            DrawRectangleRec({ b.x + w + size * 0.08f + k * size * 0.2f, b.y + size * 0.25f - k * size * 0.35f, cs, cs }, Fade(COL_BLUE, 0.6f * (1 - k)));
        }
    } else if (m.skin == "leaker") {
        // slime over the top and drips running down
        DrawRectangleRounded({ b.x + 2, b.y, w - 4, size * 0.1f }, 1, 6, GOO);
        const float xs[] = { 0.12f, 0.38f, 0.7f, 0.9f }, ls[] = { 0.30f, 0.18f, 0.42f, 0.22f };
        for (int i = 0; i < 4; i++) {
            float x = b.x + w * xs[i], len = h * ls[i] * (1 + 0.08f * sinf(m.clock * 2 + i));
            DrawRectangleRounded({ x - size * 0.04f, b.y + 2, size * 0.08f, len }, 1, 6, GOO);
            DrawCircleV({ x, b.y + 2 + len }, size * 0.055f, GOO);
        }
        float k = fmodf(m.clock * 0.7f, 1.0f);   // a drop falling off
        DrawCircleV({ b.x + w * 0.7f, b.y + h * 0.5f + k * h * 0.5f }, size * 0.035f, Fade(GOO, 1 - k * 0.5f));
    } else if (m.skin == "tux") {
        // A black jacket over the white shirt: the shirt shows in a V, with a bow tie
        Color jacket = { 30, 33, 40, 255 };
        float y0 = b.y + h * 0.76f, bottom = b.y + h;
        DrawRectangleRounded({ b.x, y0, w, bottom - y0 }, 0.6f, 10, jacket);
        DrawRectangleRec({ b.x, y0, w, (bottom - y0) * 0.4f }, jacket);   // square shoulders
        DrawTriangle({ feet.x - w * 0.2f, y0 - 1 }, { feet.x, bottom - h * 0.02f }, { feet.x + w * 0.2f, y0 - 1 }, body);
        float lt = fmaxf(1.5f, size * 0.018f);
        DrawLineEx({ feet.x - w * 0.2f, y0 }, { feet.x, bottom - h * 0.02f }, lt, { 70, 76, 88, 255 });   // lapel edges
        DrawLineEx({ feet.x + w * 0.2f, y0 }, { feet.x, bottom - h * 0.02f }, lt, { 70, 76, 88, 255 });
        float by = y0 + h * 0.035f, bs = size * 0.055f;
        DrawTriangle({ feet.x, by }, { feet.x - bs * 1.6f, by - bs }, { feet.x - bs * 1.6f, by + bs }, jacket);
        DrawTriangle({ feet.x, by }, { feet.x + bs * 1.6f, by + bs }, { feet.x + bs * 1.6f, by - bs }, jacket);
        DrawCircleV({ feet.x, by }, bs * 0.5f, jacket);
        DrawCircleV({ feet.x, by + h * 0.09f }, size * 0.016f, jacket);   // a shirt button
    } else if (m.skin == "diamond") {
        // Sparkles twinkling on the gem, and shards of diamond orbiting in front
        for (int i = 0; i < 4; i++) {
            float k = fmodf(m.clock * 0.8f + i * 0.29f, 1.0f), a = sinf(k * PI);
            Vector2 c = { b.x + w * (0.12f + 0.25f * i), b.y + h * (0.12f + 0.62f * hash01(i * 7 + (int)(m.clock * 0.8f + i * 0.29f))) };
            star4(c, size * 0.08f * a, Fade(WHITE, a));
        }
        diamond_shards(m, b, size, true);
        DrawRectangleRoundedLinesEx(b, 0.42f, 12, fmaxf(1.5f, size * 0.02f), ColorFromHSV(fmodf(m.clock * 90, 360), 0.35f, 1.0f));   // a rainbow edge
    } else if (m.skin == "glitch") {
        int beat = (int)(m.clock * 6);
        bool burst = fmodf(m.clock, 1.7f) < 0.22f;   // every so often it glitches hard
        float off = size * (burst ? 0.07f : 0.035f);
        DrawRectangleRoundedLinesEx({ b.x - off, b.y, w, h }, 0.42f, 12, line, Fade({ 255, 40, 80, 255 }, 0.7f));
        DrawRectangleRoundedLinesEx({ b.x + off, b.y, w, h }, 0.42f, 12, line, Fade({ 40, 230, 255, 255 }, 0.7f));
        // slices of the body shifted sideways
        const float ys[] = { 0.04f, 0.86f, 0.33f };
        int bands = burst ? 3 : 2;
        for (int i = 0; i < bands; i++) {
            float shift = (hash01(beat * 7 + i) - 0.5f) * size * (burst ? 0.3f : 0.14f);
            DrawRectangleRec({ b.x + size * 0.04f + shift, b.y + h * ys[i], w - size * 0.08f, h * 0.07f }, body);
        }
        for (int i = 0; i < 8; i++) {   // dead pixels
            float px = b.x - size * 0.15f + hash01(beat * 13 + i) * (w + size * 0.3f);
            float py = b.y - size * 0.1f + hash01(beat * 17 + i) * (h + size * 0.15f);
            DrawRectangleRec({ px, py, size * (0.03f + 0.03f * (i % 3)), size * 0.03f },
                             i % 2 ? Color{ 40, 230, 255, 220 } : Color{ 255, 40, 120, 220 });
        }
    }
}

// Pets: drawn about 60 units tall at the origin, then scaled into place.
static void pet_tag(const char *s, Color c)
{
    float w = ui_text_width(s, 11, FontKind::MonoBold);
    DrawRectangleRounded({ -w / 2 - 5, 6, w + 10, 16 }, 0.5f, 6, Fade(c, 0.18f));
    ui_text(s, -w / 2, 8, 11, c, FontKind::MonoBold);
}

static void draw_cat(bool tag)
{
    Color c = { 255, 205, 90, 255 }, d = darker(c, 0.75f);
    DrawLineEx({ 16, -8 }, { 30, -26 }, 5, d);
    DrawRectangleRounded({ -20, -34, 40, 34 }, 0.45f, 8, c);
    DrawTriangle({ -18, -30 }, { -4, -34 }, { -15, -46 }, c);
    DrawTriangle({ 4, -34 }, { 18, -30 }, { 15, -46 }, c);
    DrawLineEx({ -11, -20 }, { -4, -20 }, 2.5f, COL_BG);   // calm closed eyes: it never changes
    DrawLineEx({ 4, -20 }, { 11, -20 }, 2.5f, COL_BG);
    DrawCircleV({ 0, -14 }, 2, COL_CORAL);
    DrawRectangle(-4, -8, 8, 6, COL_BG);                  // padlock collar
    if (tag) pet_tag("const", c);
}

static void draw_dog(float clock, bool tag)
{
    Color c = { 200, 150, 110, 255 }, d = darker(c, 0.7f);
    float wag = sinf(clock * 12) * 6;
    DrawLineEx({ 22, -14 }, { 34, -24 + wag }, 5, d);
    DrawRectangleRounded({ -24, -32, 48, 32 }, 0.45f, 8, c);
    DrawRectangleRounded({ -30, -34, 12, 24 }, 0.8f, 6, d);   // floppy ears
    DrawRectangleRounded({ 18, -34, 12, 24 }, 0.8f, 6, d);
    DrawCircleV({ -9, -20 }, 3.5f, COL_BG);
    DrawCircleV({ 9, -20 }, 3.5f, COL_BG);
    DrawCircleV({ 0, -12 }, 4, COL_BG);
    DrawRectangleRounded({ -3, -9, 6, 8 }, 1, 6, COL_CORAL);
    if (tag) pet_tag("static", c);
}

static void draw_bird(float clock, bool tag)
{
    bool twitch = fmodf(clock, 1.3f) < 0.45f;   // it changes when you're not looking
    float jx = twitch ? sinf(clock * 37) * 3 : 0, jy = twitch ? cosf(clock * 29) * 3 : 0;
    Vector2 c = { jx, -26 + jy };
    Color col = COL_BLUE;
    DrawCircleV(c, 15, col);
    DrawTriangle({ c.x + 13, c.y - 3 }, { c.x + 13, c.y + 5 }, { c.x + 22, c.y + 1 }, COL_AMBER);
    DrawTriangle({ c.x - 6, c.y }, { c.x - 20, c.y - 12 }, { c.x - 10, c.y + 8 }, darker(col, 0.75f));
    DrawCircleV({ c.x + 5, c.y - 4 }, 3, COL_BG);
    if (twitch)
        for (int i = 0; i < 3; i++) DrawLineEx({ c.x - 30, c.y - 10 + i * 9.0f }, { c.x - 22, c.y - 10 + i * 9.0f }, 2, Fade(col, 0.6f));
    DrawLineEx({ -4, -11 }, { -4, 0 }, 2, COL_AMBER);
    DrawLineEx({ 4, -11 }, { 4, 0 }, 2, COL_AMBER);
    if (tag) pet_tag("volatile", col);
}

void mal_draw_pet(const std::string &pet, Vector2 at, float size, float clock, bool tag)
{
    if (pet != "cat" && pet != "dog" && pet != "bird") return;
    float k = size / 60.0f;
    rlPushMatrix();
    rlTranslatef(at.x, at.y, 0);
    rlScalef(k, k, 1);
    if (pet == "cat") draw_cat(tag);
    else if (pet == "dog") draw_dog(clock, tag);
    else draw_bird(clock, tag);
    rlPopMatrix();
}

void mal_draw_with_pet(const Mal &m, Vector2 feet, float size)
{
    // the pet sits on the ground beside Mal, a step behind
    mal_draw_pet(m.pet, { feet.x + size * 0.9f, feet.y }, size * 0.6f, m.clock, false);
    mal_draw(m, feet, size);
}


// The crown, for mastering every topic: five gold points tipped with pearls,
// a band of gems with a cut diamond in the middle, a shine that sweeps across,
// twinkling tips, and a golden glow behind (draw_crown_glow).
static void crown_shape(const Mal &m, Rectangle b, float size, float &cx, float &base, float &cw, float &bh, float &ph)
{
    cx = b.x + b.width / 2;
    base = b.y + b.height * 0.06f + sinf(m.clock * 2.0f) * size * 0.012f;   // it floats a little
    cw = b.width * 0.92f;
    bh = size * 0.12f;
    ph = size * 0.24f;
}

static void draw_crown_glow(const Mal &m, Rectangle b, float size)
{
    float cx, base, cw, bh, ph;
    crown_shape(m, b, size, cx, base, cw, bh, ph);
    Vector2 c = { cx, base - bh - ph * 0.3f };
    float pulse = 0.85f + 0.15f * sinf(m.clock * 3);
    BeginBlendMode(BLEND_ADDITIVE);
    float gr = size * 0.55f * pulse;
    for (int i = 0; i < 24; i++) {
        float a0 = i * 2 * PI / 24, a1 = (i + 1) * 2 * PI / 24;
        beam(c, { c.x + cosf(a0) * gr, c.y + sinf(a0) * gr * 0.75f }, { c.x + cosf(a1) * gr, c.y + sinf(a1) * gr * 0.75f }, Fade({ 255, 190, 60, 255 }, 0.5f));
    }
    for (int i = 0; i < 7; i++) {   // rays fanning up from the crown
        float a = PI + PI * (i + 0.5f) / 7 + sinf(m.clock * 0.8f + i) * 0.06f, spread = 0.06f;
        float len = size * (0.6f + 0.18f * sinf(m.clock * 2.1f + i * 1.7f));
        beam(c, { c.x + cosf(a - spread) * len, c.y + sinf(a - spread) * len }, { c.x + cosf(a + spread) * len, c.y + sinf(a + spread) * len },
             Fade({ 255, 215, 110, 255 }, 0.55f));
    }
    EndBlendMode();
}

static void gem_diamond(Vector2 c, float r, float clock)
{
    Vector2 L = { c.x - r, c.y }, R = { c.x + r, c.y }, T = { c.x, c.y - r * 0.85f }, B = { c.x, c.y + r * 1.1f };
    DrawTriangle(L, R, T, { 205, 245, 255, 255 });
    DrawTriangle(L, B, R, { 110, 195, 245, 255 });
    DrawTriangle({ c.x - r * 0.35f, c.y }, B, { c.x + r * 0.35f, c.y }, { 160, 225, 255, 255 });
    DrawTriangle({ c.x - r * 0.4f, c.y }, { c.x + r * 0.4f, c.y }, { c.x, c.y - r * 0.85f }, { 240, 252, 255, 255 });
    float k = fmodf(clock * 0.9f, 1.0f);
    star4({ c.x + r * 0.35f, c.y - r * 0.35f }, r * 1.1f * sinf(k * PI), WHITE);
}

static void draw_crown(const Mal &m, Rectangle b, float size)
{
    float cx, base, cw, bh, ph;
    crown_shape(m, b, size, cx, base, cw, bh, ph);
    Color gold = { 255, 200, 60, 255 }, deep = { 205, 140, 30, 255 }, light = { 255, 236, 150, 255 };
    float left = cx - cw / 2, bandTop = base - bh;
    float sweep = fmodf(m.clock * 0.45f, 1.8f) - 0.4f;   // the shine's position, 0..1 across
    auto shine = [&](float x) { return fmaxf(0.0f, 1.0f - fabsf((x - left) / cw - sweep) * 4.5f); };

    // Five points, the middle one tallest, each with a pearl on top
    const float hs[5] = { 0.72f, 0.88f, 1.0f, 0.88f, 0.72f };
    for (int i = 0; i < 5; i++) {
        float x0 = left + i * cw / 5, x1 = x0 + cw / 5, xm = (x0 + x1) / 2;
        Vector2 tip = { xm, bandTop - ph * hs[i] };
        DrawTriangle({ x0, bandTop + 1 }, { x1, bandTop + 1 }, tip, gold);
        DrawTriangle({ xm, bandTop + 1 }, { x1, bandTop + 1 }, tip, deep);   // the shaded side
        float sh = shine(xm);
        if (sh > 0) DrawTriangle({ x0, bandTop + 1 }, { x1, bandTop + 1 }, tip, Fade(WHITE, sh * 0.55f));
        float pr = size * 0.038f;
        DrawCircleV(tip, pr, { 250, 246, 236, 255 });
        DrawCircleV({ tip.x - pr * 0.3f, tip.y - pr * 0.3f }, pr * 0.35f, WHITE);
        float k = fmodf(m.clock * 0.7f + i * 0.21f, 1.0f);   // tips twinkle one after another
        star4({ tip.x, tip.y - pr * 1.6f }, size * 0.06f * fmaxf(0.0f, sinf(k * PI * 2)), WHITE);
    }

    // The band, with a lighter rim, slices of shine, and gems
    DrawRectangleRounded({ left - size * 0.02f, bandTop, cw + size * 0.04f, bh }, 0.35f, 6, deep);
    DrawRectangleRounded({ left, bandTop + bh * 0.12f, cw, bh * 0.76f }, 0.3f, 6, gold);
    DrawRectangleRec({ left, bandTop + bh * 0.12f, cw, bh * 0.16f }, light);
    for (int i = 0; i < 12; i++) {
        float x = left + i * cw / 12, sh = shine(x + cw / 24);
        if (sh > 0) DrawRectangleRec({ x, bandTop + bh * 0.12f, cw / 12, bh * 0.76f }, Fade(WHITE, sh * 0.6f));
    }
    float gy = bandTop + bh * 0.52f;
    for (int i = 0; i < 2; i++) {   // rubies and sapphires either side
        float d = (i + 1) * cw * 0.2f;
        Color gem = i == 0 ? COL_CORAL : Color{ 80, 140, 255, 255 };
        float gr = size * (i == 0 ? 0.035f : 0.03f);
        DrawCircleV({ cx - d, gy }, gr, gem);
        DrawCircleV({ cx + d, gy }, gr, gem);
        DrawCircleV({ cx - d - gr * 0.3f, gy - gr * 0.3f }, gr * 0.35f, Fade(WHITE, 0.9f));
        DrawCircleV({ cx + d - gr * 0.3f, gy - gr * 0.3f }, gr * 0.35f, Fade(WHITE, 0.9f));
    }
    gem_diamond({ cx, gy - size * 0.01f }, size * 0.065f, m.clock);
}

static void draw_wear(const Mal &m, Rectangle b, float size)
{
    float cx = b.x + b.width / 2, top = b.y, w = b.width, h = b.height;
    float wob = sinf(m.clock * 3.0f) * 0.04f;
    const std::string &hat = m.hat;
    if (hat == "afro") draw_afro_front(b, size);
    else if (hat == "mohawk") draw_mohawk(b, size);
    else if (hat == "drinkhat") {
        // A helmet with a can on each side and straws down to the mouth
        Color helmet = COL_CORAL;
        DrawCircleSector({ cx, top + h * 0.04f }, w * 0.5f, 180, 360, 24, helmet);
        DrawRectangleRounded({ cx - w * 0.55f, top - h * 0.01f, w * 1.1f, h * 0.07f }, 0.8f, 6, darker(helmet, 0.8f));
        Color cans[2] = { COL_BLUE, COL_AMBER };
        for (int i = 0; i < 2; i++) {
            float dir = i ? 1.0f : -1.0f;
            Rectangle can = { cx + dir * w * 0.5f - (i ? 0 : w * 0.22f), top - size * 0.26f, w * 0.22f, size * 0.3f };
            DrawLineEx({ cx + dir * w * 0.3f, top - size * 0.02f }, { can.x + can.width / 2, can.y + can.height * 0.6f }, fmaxf(2.0f, size * 0.03f), darker(helmet, 0.8f));
            DrawRectangleRounded(can, 0.3f, 6, cans[i]);
            DrawRectangleRec({ can.x, can.y + can.height * 0.4f, can.width, can.height * 0.18f }, Fade(WHITE, 0.55f));
            DrawRectangleRounded({ can.x + can.width * 0.1f, can.y - size * 0.02f, can.width * 0.8f, size * 0.04f }, 0.8f, 6, { 200, 205, 212, 255 });
            // the straw: out of the can and down to the corner of the mouth
            Vector2 s0 = { can.x + can.width / 2, can.y + can.height };
            Vector2 s3 = { cx + dir * w * 0.13f, top + h * 0.66f };
            float st = fmaxf(2.0f, size * 0.035f);
            DrawSplineSegmentBezierCubic(s0, { s0.x + dir * size * 0.08f, s0.y + h * 0.3f }, { s3.x + dir * size * 0.12f, s3.y - h * 0.02f }, s3, st, WHITE);
            DrawSplineSegmentBezierCubic(s0, { s0.x + dir * size * 0.08f, s0.y + h * 0.3f }, { s3.x + dir * size * 0.12f, s3.y - h * 0.02f }, s3, st * 0.4f, COL_CORAL);
        }
    } else if (hat == "hardhat") {
        // A proper safety helmet: dome, raised ridge down the middle, wide brim
        Color yel = { 255, 196, 40, 255 }, shade = { 222, 160, 20, 255 };
        float domeY = top + h * 0.035f, r = w * 0.52f;
        DrawRectangleRounded({ cx - w * 0.68f, domeY - h * 0.02f, w * 1.36f, h * 0.075f }, 0.9f, 8, shade);   // the brim
        DrawCircleSector({ cx, domeY }, r, 180, 360, 28, yel);
        DrawRectangleRec({ cx - r, domeY - h * 0.05f, r * 2, h * 0.05f }, shade);                             // band at the bottom of the dome
        DrawRectangleRounded({ cx - w * 0.07f, domeY - r - size * 0.02f, w * 0.14f, r + size * 0.01f }, 0.8f, 6, { 255, 222, 110, 255 });   // ridge
        DrawLineEx({ cx - r * 0.55f, domeY - r * 0.75f }, { cx - r * 0.62f, domeY - h * 0.06f }, fmaxf(1.5f, size * 0.02f), shade);
        DrawLineEx({ cx + r * 0.55f, domeY - r * 0.75f }, { cx + r * 0.62f, domeY - h * 0.06f }, fmaxf(1.5f, size * 0.02f), shade);
        DrawRing({ cx, domeY }, r * 0.72f, r * 0.8f, 205, 240, 10, Fade(WHITE, 0.55f));                      // shine
    } else if (hat == "helicopter") {
        // A propeller beanie, and the propeller really spins
        float capY = top + h * 0.1f, r = w * 0.46f;
        Color segs[4] = { COL_CORAL, COL_AMBER, COL_BLUE, COL_GREEN };
        for (int i = 0; i < 4; i++) DrawCircleSector({ cx, capY }, r, 180 + i * 45.0f, 225 + i * 45.0f, 8, segs[i]);
        DrawRectangleRounded({ cx - w * 0.5f, capY - h * 0.04f, w * 1.0f, h * 0.06f }, 0.9f, 6, { 60, 70, 90, 255 });
        float stalkTop = capY - r - size * 0.1f;
        DrawRectangleRec({ cx - size * 0.015f, stalkTop, size * 0.03f, size * 0.12f }, { 60, 70, 90, 255 });
        float spin = cosf(m.clock * 16), bl = w * 0.5f * fabsf(spin) + size * 0.02f;
        DrawEllipse((int)(cx - bl / 2), (int)stalkTop, bl / 2, size * 0.035f, spin > 0 ? COL_BLUE : COL_CORAL);
        DrawEllipse((int)(cx + bl / 2), (int)stalkTop, bl / 2, size * 0.035f, spin > 0 ? COL_CORAL : COL_BLUE);
        DrawCircleV({ cx, stalkTop }, size * 0.03f, COL_AMBER);
    } else if (hat == "wizard") {
        Color robe = { 90, 60, 170, 255 };
        Vector2 tip = { cx + w * 0.18f + wob * size * 2, top - size * 0.6f };
        DrawTriangle({ cx + w * 0.4f, top + h * 0.04f }, tip, { cx - w * 0.4f, top + h * 0.04f }, robe);
        DrawEllipse((int)cx, (int)(top + h * 0.05f), w * 0.62f, h * 0.06f, robe);
        DrawCircleV({ cx - w * 0.05f, top - size * 0.18f }, size * 0.03f, COL_AMBER);
        DrawCircleV({ cx + w * 0.12f, top - size * 0.34f }, size * 0.022f, COL_AMBER);
    } else if (hat == "crown") {
        draw_crown(m, b, size);
    } else if (hat == "sombrero") {
        // A huge straw brim, a tall crown and a zigzag band
        Color straw = { 226, 186, 104, 255 }, dark = { 186, 142, 68, 255 };
        float brimY = top + h * 0.02f;
        DrawEllipse((int)cx, (int)(brimY + size * 0.01f), w * 0.98f, size * 0.1f, dark);
        DrawEllipse((int)cx, (int)brimY, w * 0.95f, size * 0.085f, straw);
        float cw0 = w * 0.62f, cw1 = w * 0.36f, ch = size * 0.36f;
        Vector2 bl = { cx - cw0 / 2, brimY }, br = { cx + cw0 / 2, brimY }, tl = { cx - cw1 / 2, brimY - ch }, tr = { cx + cw1 / 2, brimY - ch };
        DrawTriangle(bl, br, tr, straw);
        DrawTriangle(bl, tr, tl, straw);
        DrawEllipse((int)cx, (int)(brimY - ch), cw1 / 2, size * 0.05f, straw);
        float by = brimY - ch * 0.32f, bh = size * 0.07f;   // the band, with a zigzag
        float bw = cw0 - (cw0 - cw1) * 0.32f;
        DrawRectangleRec({ cx - bw / 2, by - bh / 2, bw, bh }, COL_CORAL);
        for (int i = 0; i < 6; i++) {
            float x0 = cx - bw / 2 + i * bw / 6;
            DrawTriangle({ x0, by + bh / 2 }, { x0 + bw / 6, by + bh / 2 }, { x0 + bw / 12, by - bh / 2 }, i % 2 ? COL_GREEN : COL_AMBER);
        }
        for (int i = 0; i < 7; i++) {   // little pom-poms round the brim
            float a = PI * (0.1f + 0.8f * i / 6);
            DrawCircleV({ cx - cosf(a) * w * 0.9f, brimY + sinf(a) * size * 0.075f }, size * 0.022f, i % 2 ? COL_CORAL : COL_GREEN);
        }
    } else if (hat == "explorer") {
        // A battered brown fedora, for an adventurer
        Color felt = { 132, 88, 52, 255 }, dark = { 92, 60, 34, 255 };
        float brimY = top + h * 0.03f, tilt = size * 0.03f;
        DrawEllipse((int)cx, (int)(brimY + size * 0.012f), w * 0.8f, size * 0.075f, dark);
        DrawEllipse((int)cx, (int)brimY, w * 0.78f, size * 0.065f, felt);
        float cw0 = w * 0.66f, cw1 = w * 0.54f, ch = size * 0.24f;
        Vector2 bl = { cx - cw0 / 2, brimY }, br = { cx + cw0 / 2, brimY }, tl = { cx - cw1 / 2 + tilt, brimY - ch }, tr = { cx + cw1 / 2 + tilt, brimY - ch };
        DrawTriangle(bl, br, tr, felt);
        DrawTriangle(bl, tr, tl, felt);
        DrawEllipse((int)(cx + tilt), (int)(brimY - ch), cw1 / 2, size * 0.04f, felt);
        DrawTriangle({ cx + tilt - cw1 * 0.25f, brimY - ch }, { cx + tilt + cw1 * 0.25f, brimY - ch }, { cx + tilt, brimY - ch * 0.72f }, dark);   // the pinch
        float bh = size * 0.06f, bw = cw0 - (cw0 - cw1) * 0.2f;
        DrawRectangleRec({ cx - bw / 2 + tilt * 0.2f, brimY - size * 0.02f - bh, bw, bh }, { 40, 28, 20, 255 });
        DrawLineEx({ cx - cw0 * 0.3f, brimY - ch * 0.45f }, { cx - cw0 * 0.15f, brimY - ch * 0.8f }, fmaxf(1.5f, size * 0.015f), Fade(WHITE, 0.25f));   // a scuff
    } else if (hat == "shades") {
        // Big sunglasses, with arms back to the sides of the head
        float ey = top + h * 0.47f, ex = w * 0.22f, gw = size * 0.3f, gh = size * 0.18f;
        Color black = { 18, 20, 24, 255 };
        float th = fmaxf(2.0f, size * 0.035f);
        DrawLineEx({ b.x, ey - gh * 0.25f }, { cx - ex - gw / 2, ey - gh * 0.25f }, th, black);
        DrawLineEx({ b.x + w, ey - gh * 0.25f }, { cx + ex + gw / 2, ey - gh * 0.25f }, th, black);
        DrawRectangleRounded({ cx - ex - gw / 2, ey - gh / 2, gw, gh }, 0.55f, 8, black);
        DrawRectangleRounded({ cx + ex - gw / 2, ey - gh / 2, gw, gh }, 0.55f, 8, black);
        DrawLineEx({ cx - ex + gw / 2, ey - gh * 0.3f }, { cx + ex - gw / 2, ey - gh * 0.3f }, th, black);
        DrawLineEx({ cx - ex - gw * 0.35f, ey - gh * 0.22f }, { cx - ex - gw * 0.05f, ey - gh * 0.22f }, fmaxf(1.5f, size * 0.025f), Fade(WHITE, 0.6f));
        DrawLineEx({ cx + ex - gw * 0.35f, ey - gh * 0.22f }, { cx + ex - gw * 0.15f, ey - gh * 0.22f }, fmaxf(1.5f, size * 0.025f), Fade(WHITE, 0.6f));
    } else if (hat == "bowtie") {
        float y = top + h * 0.9f, s = size * 0.11f;
        DrawTriangle({ cx, y }, { cx - s * 1.6f, y - s }, { cx - s * 1.6f, y + s }, COL_CORAL);
        DrawTriangle({ cx, y }, { cx + s * 1.6f, y + s }, { cx + s * 1.6f, y - s }, COL_CORAL);
        DrawCircleV({ cx, y }, s * 0.45f, { 200, 80, 74, 255 });
    }
    if (m.skin == "slayer" && m.mood != MalMood::Freed) {   // a little sparkle for boss slayer gold
        float t = fmodf(m.clock * 0.8f, 1.0f);
        DrawCircleV({ b.x + w * 0.85f, top + h * (0.2f + 0.1f * t) }, size * 0.025f * (1 - t), WHITE);
    }
}

void mal_draw(const Mal &m, Vector2 feet, float size)
{
    Color body = body_color(m);
    float squash = fmaxf(0.6f, fminf(1.35f, m.squash));
    float h = size * squash;
    float w = size * 0.8f * (1.0f + (1.0f - squash) * 0.7f);   // squashed = wider
    float footH = size * 0.08f;
    float baseY = feet.y + m.hop;                                // bottom of the feet
    Rectangle b = { feet.x - w / 2, baseY - footH - h, w, h };

    // Shadow on the ground, smaller the higher Mal is
    float lift = fminf(1.0f, -m.hop / (size * 0.8f));
    DrawEllipse((int)feet.x, (int)(feet.y + 2), w * 0.5f * (1 - lift * 0.5f), size * 0.06f, Fade(BLACK, 0.35f * (1 - lift * 0.6f)));

    // ----- Crumbled (or crumbling, or building back up): draw cells instead -----
    if (m.crumble > 0.001f) {
        const int cols = 6, rows = 7;
        float cw = w / cols, ch = h / rows;
        float c = m.crumble;
        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                int id = row * cols + col;
                float r1 = hash01(id), r2 = hash01(id + 99);
                // Each cell starts falling a little later than the one below it
                float start = (rows - 1 - row) * 0.06f + r1 * 0.15f;
                float k = fmaxf(0.0f, fminf(1.0f, (c - start) / (1.0f - start)));
                float x = b.x + col * cw + (r2 - 0.5f) * size * 0.9f * k;
                float yHome = b.y + row * ch;
                float yFloor = feet.y - ch * (0.6f + r1 * 1.4f);   // they pile up unevenly
                float y = yHome + (yFloor - yHome) * k * k;
                Color cc = (row == 0 && col % 2 == 0) ? darker(body, 0.8f) : body;
                DrawRectangleRec({ x + 1, y + 1, cw - 2, ch - 2 }, Fade(cc, 1.0f - 0.25f * k));
                if (k > 0.95f && (id % 5 == 0))   // a few cells show their bytes
                    ui_text("0", x + cw * 0.3f, y, ch * 0.8f, COL_BG, FontKind::Mono);
            }
        }
        if (c < 0.35f) {   // the face is still there as it starts to go
            float ex = w * 0.2f, ey = b.y + h * 0.45f, er = size * 0.07f;
            draw_eye(m, { feet.x - ex, ey }, er, true);
            draw_eye(m, { feet.x + ex, ey }, er, false);
        }
        return;
    }

    draw_skin_back(m, b, feet, size, body);
    if (m.hat == "afro") draw_afro_back(b, size);
    if (m.hat == "crown" && m.mood != MalMood::Freed) draw_crown_glow(m, b, size);
    bool hatCovers = m.hat == "afro" || m.hat == "mohawk" || m.hat == "drinkhat" || m.hat == "hardhat" || m.hat == "helicopter" || m.hat == "sombrero" || m.hat == "explorer" || m.hat == "wizard" || m.hat == "crown";

    // ----- Antenna: a pointer arrow on top, wobbling as Mal moves -----
    if (!hatCovers) {
    float wob = sinf(m.clock * 3.0f) * 0.12f + m.hopVel * 0.0004f - (squash - 1) * 0.8f;
    Vector2 root = { feet.x, b.y + size * 0.02f };
    float len = size * 0.24f;
    Vector2 tip = { root.x + sinf(wob) * len, root.y - cosf(wob) * len };
    float at = fmaxf(2.0f, size * 0.045f);
    DrawLineEx(root, tip, at, COL_BLUE);
    float hx = sinf(wob), hy = -cosf(wob), hs = size * 0.09f;
    DrawLineEx(tip, { tip.x - hx * hs - hy * hs * 0.8f, tip.y - hy * hs + hx * hs * 0.8f }, at, COL_BLUE);
    DrawLineEx(tip, { tip.x - hx * hs + hy * hs * 0.8f, tip.y - hy * hs - hx * hs * 0.8f }, at, COL_BLUE);
    DrawCircleV(tip, at / 2, COL_BLUE);
    }

    // ----- Feet -----
    float fw = w * 0.24f;
    DrawRectangleRounded({ feet.x - w * 0.3f - fw / 2, baseY - footH * 1.4f, fw, footH * 1.4f }, 0.6f, 6, darker(body, 0.7f));
    DrawRectangleRounded({ feet.x + w * 0.3f - fw / 2, baseY - footH * 1.4f, fw, footH * 1.4f }, 0.6f, 6, darker(body, 0.7f));

    // ----- Body: a darker base for depth, then the block itself -----
    DrawRectangleRounded({ b.x, b.y + size * 0.05f, b.width, b.height }, 0.42f, 12, darker(body, 0.72f));
    DrawRectangleRounded(b, 0.42f, 12, body);
    if (m.skin == "diamond" && m.mood != MalMood::Freed) diamond_facets(m, b, size);

    // Its address, like a label on a memory block (only when big enough to read)
    if (size >= 70) {
        Rectangle plate = { b.x + w * 0.16f, b.y + h * 0.1f, w * 0.68f, h * 0.17f };
        DrawRectangleRounded(plate, 0.4f, 6, darker(body, 0.82f));
        float fs = plate.height * 0.72f;
        std::string addr = mal_address(m);
        ui_text(addr, plate.x + (plate.width - ui_text_width(addr, fs, FontKind::MonoBold)) / 2, plate.y + plate.height * 0.1f,
                fs, m.skin == "dangling" ? COL_CORAL : COL_BG, FontKind::MonoBold);
    }

    // ----- Face -----
    float ex = w * 0.2f, ey = b.y + h * 0.47f, er = size * 0.07f;
    draw_eye(m, { feet.x - ex, ey }, er, true);
    draw_eye(m, { feet.x + ex, ey }, er, false);
    draw_mouth(m, { feet.x, b.y + h * 0.67f }, w * 0.42f, body);

    if (m.mood == MalMood::Happy || m.mood == MalMood::Cheer) {   // rosy cheeks
        DrawCircleV({ feet.x - w * 0.33f, b.y + h * 0.62f }, size * 0.055f, Fade(COL_CORAL, 0.55f));
        DrawCircleV({ feet.x + w * 0.33f, b.y + h * 0.62f }, size * 0.055f, Fade(COL_CORAL, 0.55f));
    }
    if (m.mood == MalMood::Sad) {   // a tear that runs down and starts again
        float t = fmodf(m.clock, 1.2f) / 1.2f;
        Vector2 d = { feet.x + ex + er * 0.3f, ey + er + t * h * 0.35f };
        DrawCircleV(d, size * 0.035f, Fade(COL_BLUE, 1.0f - t));
    }
    if (m.mood == MalMood::Worried) {   // a sweat drop on the side of the head
        Vector2 d = { b.x + w * 0.9f, b.y + h * 0.28f + sinf(m.clock * 4) * 2 };
        DrawCircleV(d, size * 0.045f, COL_BLUE);
        DrawTriangle({ d.x, d.y - size * 0.09f }, { d.x - size * 0.04f, d.y - size * 0.01f }, { d.x + size * 0.04f, d.y - size * 0.01f }, COL_BLUE);
    }
    draw_skin_front(m, b, feet, size, body);
    draw_wear(m, b, size);

    if (m.mood == MalMood::Sleep) {   // zzz drifting up
        for (int i = 0; i < 3; i++) {
            float t = fmodf(m.clock * 0.5f + i / 3.0f, 1.0f);
            float zs = size * (0.12f + 0.08f * t);
            ui_text("z", b.x + w + size * 0.05f + t * size * 0.3f, b.y - t * size * 0.5f, zs, Fade(COL_TEXT_SOFT, 1 - t), FontKind::MonoBold);
        }
    }
}

void mal_say(const std::string &text, float x, float y, float width, Vector2 tail)
{
    float pad = 14, size = 15;
    float th = ui_text_wrapped_height(text, width - 2 * pad, size);
    Rectangle r = { x, y, width, th + 2 * pad - 4 };

    // The tail: a triangle from the nearest edge of the bubble towards Mal
    float cx = fmaxf(r.x + 18, fminf(r.x + r.width - 18, tail.x));
    bool below = tail.y > r.y + r.height;
    float edge = below ? r.y + r.height : r.y;
    Vector2 a = { cx - 9, edge }, b = { cx + 9, edge };
    if (below) DrawTriangle(a, tail, b, COL_LINE); else DrawTriangle(b, tail, a, COL_LINE);
    ui_panel(r, COL_SURFACE_2, COL_LINE, 1);
    Vector2 a2 = { cx - 7, edge + (below ? -1.5f : 1.5f) }, b2 = { cx + 7, edge + (below ? -1.5f : 1.5f) };
    Vector2 t2 = { tail.x + (cx - tail.x) * 0.12f, tail.y + (edge - tail.y) * 0.12f };
    if (below) DrawTriangle(a2, t2, b2, COL_SURFACE_2); else DrawTriangle(b2, t2, a2, COL_SURFACE_2);

    ui_text_wrapped(text, r.x + pad, r.y + pad - 2, width - 2 * pad, size, COL_TEXT);
}
