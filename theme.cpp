// theme.cpp - the colours of each app theme, and the extras each one draws:
// a background behind the screens, something round the big panels, and an
// overlay on top. What unlocks them is in apptheme.cpp.
//
//   Stack Trace  the normal dark look (nothing extra)
//   Phosphor     an old green-screen terminal: falling hex behind, scanlines
//                and a dark curved edge on top, like the glass of a CRT
//   Neon Night   a synthwave sunset with a moving neon grid, panels lit with
//                pink and cyan tubes
//   Paper        a light notebook page: ruled lines and a margin, panels
//                taped on with a pencil outline and a shadow
//   Blueprint    grid paper, corner marks and measurements round the panels,
//                and a title block on the title screen

#include "theme.h"
#include "ui.h"
#include <cmath>
#include <cstdio>

// The colours every screen uses. theme_apply() swaps them.
Color COL_BG, COL_SURFACE, COL_SURFACE_2, COL_LINE, COL_LINE_SOFT;
Color COL_TEXT, COL_TEXT_SOFT, COL_MUTED;
Color COL_GREEN, COL_AMBER, COL_CORAL, COL_BLUE, COL_PURPLE;

struct Palette {
    Color bg, surface, surface2, line, lineSoft;
    Color text, textSoft, muted;
    Color green, amber, coral, blue, purple;
};

enum class Look { Normal, Phosphor, Neon, Paper, Blueprint };

static Palette palette_for(Look l)
{
    switch (l) {
    case Look::Phosphor:
        return { { 3, 10, 5, 255 }, { 7, 19, 10, 255 }, { 10, 27, 14, 255 }, { 28, 72, 38, 255 }, { 15, 38, 20, 255 },
                 { 178, 255, 178, 255 }, { 140, 232, 148, 255 }, { 76, 156, 92, 255 },
                 { 110, 255, 120, 255 }, { 214, 255, 96, 255 }, { 255, 112, 92, 255 }, { 96, 255, 210, 255 }, { 190, 255, 150, 255 } };
    case Look::Neon:
        return { { 17, 7, 33, 255 }, { 29, 13, 52, 255 }, { 39, 18, 66, 255 }, { 92, 46, 128, 255 }, { 52, 25, 82, 255 },
                 { 248, 234, 255, 255 }, { 222, 202, 242, 255 }, { 156, 124, 186, 255 },
                 { 70, 255, 204, 255 }, { 255, 196, 84, 255 }, { 255, 86, 160, 255 }, { 88, 212, 255, 255 }, { 204, 128, 255, 255 } };
    case Look::Paper:
        return { { 245, 240, 229, 255 }, { 254, 251, 244, 255 }, { 236, 229, 214, 255 }, { 205, 194, 172, 255 }, { 226, 218, 200, 255 },
                 { 38, 34, 28, 255 }, { 70, 63, 54, 255 }, { 132, 121, 104, 255 },
                 { 36, 148, 72, 255 }, { 204, 118, 16, 255 }, { 208, 66, 56, 255 }, { 36, 108, 200, 255 }, { 128, 78, 190, 255 } };
    case Look::Blueprint:
        return { { 14, 40, 88, 255 }, { 19, 51, 107, 255 }, { 25, 61, 124, 255 }, { 92, 142, 212, 255 }, { 46, 86, 152, 255 },
                 { 238, 245, 255, 255 }, { 204, 222, 246, 255 }, { 142, 172, 216, 255 },
                 { 140, 255, 190, 255 }, { 255, 214, 108, 255 }, { 255, 140, 128, 255 }, { 164, 222, 255, 255 }, { 214, 192, 255, 255 } };
    default:
        return { { 13, 17, 23, 255 }, { 22, 27, 34, 255 }, { 28, 33, 40, 255 }, { 48, 54, 61, 255 }, { 33, 38, 45, 255 },
                 { 230, 237, 243, 255 }, { 201, 209, 217, 255 }, { 139, 148, 158, 255 },
                 { 126, 231, 135, 255 }, { 255, 180, 84, 255 }, { 255, 123, 114, 255 }, { 121, 192, 255, 255 }, { 210, 168, 255, 255 } };
    }
}

static Look look_for(const std::string &id)
{
    if (id == "phosphor") return Look::Phosphor;
    if (id == "neon") return Look::Neon;
    if (id == "paper") return Look::Paper;
    if (id == "blueprint") return Look::Blueprint;
    return Look::Normal;
}

static Look look = Look::Normal;
static std::string currentId = "default";

// Set the colours before anything draws (main calls theme_apply again once
// the save is loaded)
static struct ApplyDefault { ApplyDefault() { theme_apply("default"); } } applyDefault;

void theme_apply(const std::string &id)
{
    look = look_for(id);
    currentId = look == Look::Normal ? "default" : id;
    Palette p = palette_for(look);
    COL_BG = p.bg; COL_SURFACE = p.surface; COL_SURFACE_2 = p.surface2; COL_LINE = p.line; COL_LINE_SOFT = p.lineSoft;
    COL_TEXT = p.text; COL_TEXT_SOFT = p.textSoft; COL_MUTED = p.muted;
    COL_GREEN = p.green; COL_AMBER = p.amber; COL_CORAL = p.coral; COL_BLUE = p.blue; COL_PURPLE = p.purple;
}

const std::string &theme_current() { return currentId; }

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

static float noise(int n)   // 0..1, the same every frame for the same n
{
    unsigned int h = (unsigned int)n * 374761393u + 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (h ^ (h >> 16)) / 4294967296.0f;
}

static bool same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

static Color mix(Color a, Color b, float k)
{
    return { (unsigned char)(a.r + (b.r - a.r) * k), (unsigned char)(a.g + (b.g - a.g) * k),
             (unsigned char)(a.b + (b.b - a.b) * k), 255 };
}

static float roundness(Rectangle r, float radius)
{
    float s = r.width < r.height ? r.width : r.height;
    return s <= 0 ? 0 : fminf(1, 2 * radius / s);
}

static Rectangle grow(Rectangle r, float d) { return { r.x - d, r.y - d, r.width + 2 * d, r.height + 2 * d }; }

static int rect_hash(Rectangle r) { return (int)r.x * 73 + (int)r.y * 151 + (int)r.width * 7; }

// Only the big panels (the screen's main boxes) get dressed up
static bool big_panel(Rectangle r, Color fill)
{
    return same(fill, COL_SURFACE) && r.width >= 220 && r.height >= 70;
}

const Color NEON_PINK = { 255, 60, 170, 255 }, NEON_CYAN = { 60, 220, 255, 255 };

// ---------------------------------------------------------------------------
// Backgrounds
// ---------------------------------------------------------------------------

static void phosphor_background(float t, Rectangle area)
{
    static const char *hex = "0123456789ABCDEF";
    for (float x = area.x + 8; x < area.x + area.width; x += 22) {
        int col = (int)(x - area.x);
        float speed = 30 + noise(col) * 70;
        int length = 6 + (int)(noise(col * 3) * 16);
        float span = area.height + length * 16;
        float head = fmodf(noise(col * 7) * 2000 + t * speed, span) - length * 16 + area.y;
        for (int j = 0; j <= length; j++) {
            float y = head + j * 16;
            if (y < area.y - 16 || y > area.y + area.height) continue;
            char ch[2] = { hex[(int)(noise(col * 31 + j + (int)(t * 1.5f + noise(j) * 4)) * 16)], 0 };
            if (j == length) ui_text(noise(col + j) < 0.5f ? "0" : "1", x, y, 13, Fade(COL_GREEN, 0.75f), FontKind::Mono);
            else ui_text(ch, x, y, 13, Fade(COL_GREEN, 0.04f + 0.22f * j / length), FontKind::Mono);
        }
    }
}

static void neon_background(float t, Rectangle a)
{
    float hz = a.y + a.height * 0.625f;
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, (int)(hz - a.y) / 2 + 1, { 12, 4, 30, 255 }, { 34, 8, 58, 255 });
    DrawRectangleGradientV((int)a.x, (int)(a.y + (hz - a.y) / 2), (int)a.width, (int)(hz - a.y) / 2 + 1, { 34, 8, 58, 255 }, { 120, 20, 110, 255 });
    // stars
    for (int i = 0; i < 90; i++) {
        float x = a.x + noise(i * 3) * a.width, y = a.y + noise(i * 5 + 1) * (hz - a.y - 60);
        float tw = 0.4f + 0.6f * fabsf(sinf(t * (0.6f + noise(i)) + i));
        DrawRectangle((int)x, (int)y, 2, 2, Fade(WHITE, tw * 0.8f));
    }
    // the sun, striped near the bottom
    float r = a.height * 0.17f, cx = a.x + a.width * 0.72f, cy = hz - r * 0.33f;
    for (int y = (int)(cy - r); y < (int)hz; y++) {
        float dy = y - cy, half = sqrtf(fmaxf(0, r * r - dy * dy));
        if (half <= 0) continue;
        float below = (y - (cy - r * 0.2f)) / (r * 0.6f);
        if (below > 0 && fmodf(y - cy + t * 6, 14) < 3 + below * 5) continue;
        float k = (y - (cy - r)) / (hz - (cy - r));
        DrawRectangle((int)(cx - half), y, (int)(half * 2), 1, mix({ 255, 220, 90, 255 }, { 255, 60, 150, 255 }, k));
    }
    // the floor and its grid, rushing towards you
    DrawRectangle((int)a.x, (int)hz, (int)a.width, (int)(a.y + a.height - hz) + 1, { 30, 6, 50, 255 });
    float floorH = a.y + a.height - hz;
    for (int i = 0; i < 14; i++) {
        float u = (i + fmodf(t * 0.6f, 1.0f)) / 14.0f;
        float y = hz + floorH * u * u;
        DrawRectangle((int)a.x, (int)y, (int)a.width, 1, Fade(NEON_PINK, 0.25f + 0.45f * u));
    }
    float mid = a.x + a.width / 2;
    for (int i = -30; i <= 30; i++)
        DrawLineEx({ mid + i * a.width * 0.0094f, hz }, { mid + i * a.width * 0.29f, a.y + a.height }, 1.2f, Fade(NEON_PINK, 0.55f));
    DrawRectangle((int)a.x, (int)hz - 1, (int)a.width, 3, { 255, 140, 230, 255 });
}

static void paper_background(Rectangle a, float gap)
{
    Color rule = mix(COL_BG, { 150, 190, 230, 255 }, 0.45f);
    for (float y = a.y + gap; y < a.y + a.height; y += gap) DrawRectangle((int)a.x, (int)y, (int)a.width, 1, rule);
    DrawRectangle((int)(a.x + gap), (int)a.y, 1, (int)a.height, { 230, 120, 120, 255 });
}

static void blueprint_background(Rectangle a, float fine)
{
    Color f = mix(COL_BG, { 60, 110, 190, 255 }, 0.18f), m = mix(COL_BG, { 90, 150, 230, 255 }, 0.45f);
    int n = 0;
    for (float x = a.x; x < a.x + a.width; x += fine, n++) DrawRectangle((int)x, (int)a.y, 1, (int)a.height, n % 5 ? f : m);
    n = 0;
    for (float y = a.y; y < a.y + a.height; y += fine, n++) DrawRectangle((int)a.x, (int)y, (int)a.width, 1, n % 5 ? f : m);
}

void theme_background(float t)
{
    Rectangle all = { 0, 0, (float)SCREEN_W, (float)SCREEN_H };
    switch (look) {
        case Look::Phosphor:  phosphor_background(t, all); break;
        case Look::Neon:      neon_background(t, all); break;
        case Look::Paper:     paper_background(all, 26); break;
        case Look::Blueprint: blueprint_background(all, 10); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Round the big panels
// ---------------------------------------------------------------------------

static Color neon_colour(Rectangle r, Color border)
{
    if (!same(border, COL_LINE)) return border;
    return noise(rect_hash(r)) < 0.5f ? NEON_PINK : NEON_CYAN;
}

void theme_panel_under(Rectangle r, Color fill, Color border)
{
    if (look == Look::Normal || look == Look::Phosphor || !big_panel(r, fill)) return;
    if (look == Look::Neon) {   // a soft glow round the tube
        Color c = neon_colour(r, border);
        for (int i = 5; i >= 1; i--) {
            Rectangle g = grow(r, i * 2.5f);
            DrawRectangleRoundedLinesEx(g, roundness(g, 8 + i * 2.5f), 8, 3, Fade(c, 0.07f + 0.02f * (5 - i)));
        }
    } else if (look == Look::Paper) {   // a shadow down and to the right
        for (int i = 0; i < 4; i++) {
            Rectangle s = { r.x + 4 + i, r.y + 5 + i, r.width, r.height };
            DrawRectangleRounded(s, roundness(s, 8), 8, Fade({ 90, 70, 40, 255 }, 0.05f));
        }
    }
}

static void tape(float cx, float cy, float angle)
{
    DrawRectanglePro({ cx, cy, 56, 18 }, { 28, 9 }, angle, Fade({ 240, 220, 140, 255 }, 0.68f));
    DrawRectanglePro({ cx, cy, 56, 2 }, { 28, 9 }, angle, Fade({ 200, 170, 90, 255 }, 0.35f));
}

static void pencil_rect(Rectangle r, int seed)
{
    Vector2 c[4] = { { r.x, r.y }, { r.x + r.width, r.y }, { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
    Color lead = Fade({ 60, 60, 70, 255 }, 0.55f);
    int n = seed;
    for (int s = 0; s < 4; s++) {
        Vector2 a = c[s], b = c[(s + 1) % 4];
        float len = fmaxf(fabsf(b.x - a.x), fabsf(b.y - a.y));
        int steps = (int)(len / 30) + 2;
        Vector2 prev = a;
        for (int i = 1; i <= steps; i++) {
            float u = (float)i / steps;
            Vector2 p = { a.x + (b.x - a.x) * u + (noise(++n) - 0.5f) * 3.2f, a.y + (b.y - a.y) * u + (noise(++n) - 0.5f) * 3.2f };
            DrawLineEx(prev, p, 1.6f, lead);
            prev = p;
        }
    }
}

static void corner_marks(Rectangle r, Color c)
{
    const float gap = 4, len = 16;
    float xs[2] = { r.x, r.x + r.width }, ys[2] = { r.y, r.y + r.height };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) {
            float sx = i ? 1 : -1, sy = j ? 1 : -1;
            DrawLineEx({ xs[i] + sx * gap, ys[j] }, { xs[i] + sx * (gap + len), ys[j] }, 1, c);
            DrawLineEx({ xs[i], ys[j] + sy * gap }, { xs[i], ys[j] + sy * (gap + len) }, 1, c);
        }
}

static void dimension_line(Rectangle r, Color c)
{
    float y = r.y - 12;
    DrawLineEx({ r.x, y }, { r.x + r.width, y }, 1, c);
    for (int s = 0; s < 2; s++) {
        float x = s ? r.x + r.width : r.x, d = s ? -7.0f : 7.0f;
        ui_triangle({ x, y }, { x + d, y - 3 }, { x + d, y + 3 }, c);
        DrawLineEx({ x, y - 5 }, { x, y + 5 }, 1, c);
    }
    char label[24];
    snprintf(label, sizeof label, "%d px", (int)r.width);
    float tw = ui_text_width(label, 10, FontKind::Mono);
    DrawRectangle((int)(r.x + r.width / 2 - tw / 2 - 5), (int)(y - 7), (int)tw + 10, 14, COL_BG);
    ui_text(label, r.x + r.width / 2 - tw / 2, y - 6, 10, c, FontKind::Mono);
}

void theme_panel_over(Rectangle r, Color fill, Color border)
{
    if (look == Look::Normal || look == Look::Phosphor || !big_panel(r, fill)) return;
    if (look == Look::Neon) {
        Color c = neon_colour(r, border);
        DrawRectangleRoundedLinesEx(r, roundness(r, 8), 8, 2.5f, c);
        Rectangle in = grow(r, -2);
        DrawRectangleRoundedLinesEx(in, roundness(in, 6), 8, 1, Fade(WHITE, 0.35f));
    } else if (look == Look::Paper) {
        pencil_rect(r, rect_hash(r));
        tape(r.x + 4, r.y + 3, -38);   // across the top corners
        tape(r.x + r.width - 4, r.y + 3, 38);
    } else if (look == Look::Blueprint) {
        Color ink = { 225, 238, 255, 255 };
        corner_marks(r, ink);
        if (r.y >= 110 && r.width >= 600) dimension_line(r, ink);   // only the widest: it needs room above
    }
}

// ---------------------------------------------------------------------------
// On top of everything
// ---------------------------------------------------------------------------

static void blueprint_title_block()
{
    Color ink = { 225, 238, 255, 255 };
    Rectangle b = { (float)MARGIN, 24, 256, 52 };   // top left, where the title screen has room
    DrawRectangleRec(b, Fade(COL_BG, 0.85f));
    DrawRectangleLinesEx(b, 1.5f, ink);
    DrawLineEx({ b.x, b.y + 20 }, { b.x + b.width, b.y + 20 }, 1, ink);
    DrawLineEx({ b.x + 120, b.y + 20 }, { b.x + 120, b.y + b.height }, 1, ink);
    ui_text("STACK_TRACE // GENERAL ARRANGEMENT", b.x + 7, b.y + 4, 10, ink, FontKind::MonoBold);
    ui_text("DRAWN: MAL", b.x + 7, b.y + 29, 10, ink, FontKind::Mono);
    ui_text("SCALE 1:1  SHEET 1/1", b.x + 127, b.y + 29, 10, ink, FontKind::Mono);
}

void theme_overlay(float t, bool titleBlock)
{
    if (look == Look::Phosphor) {
        // scanlines, a faint flicker, dark edges and the curved glass's corners
        for (int y = 0; y < SCREEN_H; y += 3) DrawRectangle(0, y, SCREEN_W, 1, Fade(BLACK, 0.26f));
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(COL_GREEN, 0.012f + 0.008f * sinf(t * 40)));
        const int e = 150;
        DrawRectangleGradientH(0, 0, e, SCREEN_H, Fade(BLACK, 0.55f), Fade(BLACK, 0));
        DrawRectangleGradientH(SCREEN_W - e, 0, e, SCREEN_H, Fade(BLACK, 0), Fade(BLACK, 0.55f));
        DrawRectangleGradientV(0, 0, SCREEN_W, e / 2, Fade(BLACK, 0.5f), Fade(BLACK, 0));
        DrawRectangleGradientV(0, SCREEN_H - e / 2, SCREEN_W, e / 2, Fade(BLACK, 0), Fade(BLACK, 0.5f));
        Rectangle glass = { -30, -30, SCREEN_W + 60.0f, SCREEN_H + 60.0f };
        DrawRectangleRoundedLinesEx(glass, 0.16f, 16, 40, BLACK);
    } else if (look == Look::Blueprint && titleBlock) {
        blueprint_title_block();
    }
}

// ---------------------------------------------------------------------------
// The shop's preview: a tiny title screen in the theme's colours
// ---------------------------------------------------------------------------

void theme_preview(const std::string &id, Rectangle r, float t)
{
    // Draw it with the theme's own colours, then put the current ones back
    std::string was = currentId;
    theme_apply(id);
    DrawRectangleRec(r, COL_BG);
    ui_scissor((int)r.x, (int)r.y, (int)r.width, (int)r.height);
    switch (look) {
        case Look::Phosphor:  phosphor_background(t, r); break;
        case Look::Neon:      neon_background(t, r); break;
        case Look::Paper:     paper_background(r, 14); break;
        case Look::Blueprint: blueprint_background(r, 8); break;
        default: break;
    }
    Rectangle panel = { r.x + r.width * 0.52f, r.y + 14, r.width * 0.42f, r.height - 28 };
    theme_panel_under(panel, COL_SURFACE, COL_LINE);
    DrawRectangleRounded(panel, roundness(panel, 6), 6, COL_LINE);
    Rectangle in = grow(panel, -1);
    DrawRectangleRounded(in, roundness(in, 5), 6, COL_SURFACE);
    if (look == Look::Neon) DrawRectangleRoundedLinesEx(panel, roundness(panel, 6), 6, 2, NEON_CYAN);
    if (look == Look::Paper) { pencil_rect(panel, 9); tape(panel.x + 3, panel.y + 2, -38); }
    if (look == Look::Blueprint) corner_marks(panel, { 225, 238, 255, 255 });
    ui_text("int *p = &x;", panel.x + 8, panel.y + 12, 10, COL_TEXT, FontKind::Mono);
    ui_text("*p = 7;", panel.x + 8, panel.y + 26, 10, COL_TEXT_SOFT, FontKind::Mono);
    DrawRectangle((int)panel.x + 8, (int)(panel.y + panel.height - 22), (int)(panel.width * 0.5f), 12, Fade(COL_AMBER, 0.85f));
    float lx = r.x + 12;
    ui_text("STACK TRACE", lx, r.y + 18, 14, COL_TEXT, FontKind::MonoBold);
    Rectangle b1 = { lx, r.y + 44, r.width * 0.2f, 16 }, b2 = { lx + r.width * 0.22f, r.y + 44, r.width * 0.2f, 16 };
    DrawRectangleRounded(b1, 0.4f, 6, COL_AMBER);
    DrawRectangleRounded(b2, 0.4f, 6, COL_GREEN);
    DrawRectangleRoundedLinesEx({ lx, r.y + 66, r.width * 0.2f, 16 }, 0.4f, 6, 1.5f, COL_PURPLE);
    DrawRectangleRoundedLinesEx({ lx + r.width * 0.22f, r.y + 66, r.width * 0.2f, 16 }, 0.4f, 6, 1.5f, COL_CORAL);
    if (look == Look::Phosphor)
        for (float y = r.y; y < r.y + r.height; y += 3) DrawRectangle((int)r.x, (int)y, (int)r.width, 1, Fade(BLACK, 0.25f));
    EndScissorMode();
    theme_apply(was);
}
