// mascot.cpp - drawing and animating Mal.

#include "mascot.h"
#include "theme.h"
#include "ui.h"
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

static Color body_color(MalMood mood)
{
    switch (mood) {
        case MalMood::Worried: return COL_AMBER;
        case MalMood::Sad:     return { 110, 190, 140, 255 };   // a duller green
        case MalMood::Freed:   return { 120, 128, 138, 255 };
        default:               return COL_GREEN;
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

void mal_draw(const Mal &m, Vector2 feet, float size)
{
    Color body = body_color(m.mood);
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

    // ----- Antenna: a pointer arrow on top, wobbling as Mal moves -----
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

    // ----- Feet -----
    float fw = w * 0.24f;
    DrawRectangleRounded({ feet.x - w * 0.3f - fw / 2, baseY - footH * 1.4f, fw, footH * 1.4f }, 0.6f, 6, darker(body, 0.7f));
    DrawRectangleRounded({ feet.x + w * 0.3f - fw / 2, baseY - footH * 1.4f, fw, footH * 1.4f }, 0.6f, 6, darker(body, 0.7f));

    // ----- Body: a darker base for depth, then the block itself -----
    DrawRectangleRounded({ b.x, b.y + size * 0.05f, b.width, b.height }, 0.42f, 12, darker(body, 0.72f));
    DrawRectangleRounded(b, 0.42f, 12, body);

    // Its address, like a label on a memory block (only when big enough to read)
    if (size >= 70) {
        Rectangle plate = { b.x + w * 0.16f, b.y + h * 0.1f, w * 0.68f, h * 0.17f };
        DrawRectangleRounded(plate, 0.4f, 6, darker(body, 0.82f));
        float fs = plate.height * 0.72f;
        const char *addr = "0x9000";
        ui_text(addr, plate.x + (plate.width - ui_text_width(addr, fs, FontKind::MonoBold)) / 2, plate.y + plate.height * 0.1f,
                fs, COL_BG, FontKind::MonoBold);
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
