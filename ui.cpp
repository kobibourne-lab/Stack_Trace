// ui.cpp - shared drawing pieces. See ui.h.

#include "ui.h"
#include "sfx.h"
#include <cmath>
#include <sstream>

// ---------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------

static Font fonts[4];
static bool loaded[4] = { false, false, false, false };
static bool wantHand = false;   // something clickable is under the mouse this frame
static bool wantIbeam = false;  // editable text is under the mouse this frame

static const char *fontFiles[4] = {
    "fonts/IBMPlexSans-Regular.ttf",
    "fonts/IBMPlexSans-SemiBold.ttf",
    "fonts/JetBrainsMono-Regular.ttf",
    "fonts/JetBrainsMono-Bold.ttf",
};

void ui_load_fonts()
{
    for (int i = 0; i < 4; i++) {
        if (FileExists(fontFiles[i])) {
            // Loaded large and scaled down. The mipmaps keep small text (11-13 px)
            // readable; without them a 0 can look like a 3.
            fonts[i] = LoadFontEx(fontFiles[i], 64, nullptr, 0);
            GenTextureMipmaps(&fonts[i].texture);
            SetTextureFilter(fonts[i].texture, TEXTURE_FILTER_TRILINEAR);
            loaded[i] = true;
        } else {
            TraceLog(LOG_WARNING, "UI: %s not found, using the built-in font", fontFiles[i]);
            fonts[i] = GetFontDefault();
            loaded[i] = false;
        }
    }
}

void ui_unload_fonts()
{
    for (int i = 0; i < 4; i++)
        if (loaded[i]) UnloadFont(fonts[i]);
}

static Font &font_for(FontKind kind) { return fonts[(int)kind]; }

// The built-in font needs letter spacing; the TTF fonts don't.
static float spacing_for(FontKind kind, float size)
{
    return loaded[(int)kind] ? 0.0f : size / 10.0f;
}

// ---------------------------------------------------------------------------
// Scaling to the window
// ---------------------------------------------------------------------------

static Camera2D currentFit = { { 0, 0 }, { 0, 0 }, 0, 1 };

void ui_set_fit(Camera2D fit) { currentFit = fit; }

// BeginScissorMode works in real window pixels and ignores the camera, so the
// box is scaled and moved the same way the camera moves everything else.
void ui_scissor(int x, int y, int width, int height)
{
    float s = currentFit.zoom;
    BeginScissorMode((int)(currentFit.offset.x + x * s), (int)(currentFit.offset.y + y * s),
                     (int)(width * s + 0.5f), (int)(height * s + 0.5f));
}

void ui_begin_frame() { wantHand = false; wantIbeam = false; }

void ui_want_hand() { wantHand = true; }

void ui_want_ibeam() { wantIbeam = true; }

void ui_end_frame()
{
    SetMouseCursor(wantHand ? MOUSE_CURSOR_POINTING_HAND : (wantIbeam ? MOUSE_CURSOR_IBEAM : MOUSE_CURSOR_DEFAULT));
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

void ui_text(const std::string &text, float x, float y, float size, Color color, FontKind font)
{
    DrawTextEx(font_for(font), text.c_str(), { x, y }, size, spacing_for(font, size), color);
}

float ui_text_width(const std::string &text, float size, FontKind font)
{
    return MeasureTextEx(font_for(font), text.c_str(), size, spacing_for(font, size)).x;
}

// Lays out wrapped text; draws it only if draw is true. Returns the height.
static float wrap(const std::string &text, float x, float y, float width, float size, Color color,
                  FontKind font, bool draw);

float ui_text_wrapped(const std::string &text, float x, float y, float width, float size, Color color,
                      FontKind font)
{
    return wrap(text, x, y, width, size, color, font, true);
}

float ui_text_wrapped_height(const std::string &text, float width, float size, FontKind font)
{
    return wrap(text, 0, 0, width, size, BLANK, font, false);
}

static float wrap(const std::string &text, float x, float y, float width, float size, Color color,
                  FontKind font, bool draw)
{
    float lineH = size * 1.45f;
    float cy = y;
    std::stringstream paragraphs(text);
    std::string para;
    bool first = true;

    while (std::getline(paragraphs, para)) {
        if (!first) cy += size * 0.5f;   // a little space between paragraphs
        first = false;

        std::stringstream words(para);
        std::string word, line;
        while (words >> word) {
            std::string tryLine = line.empty() ? word : line + " " + word;
            if (!line.empty() && ui_text_width(tryLine, size, font) > width) {
                if (draw) ui_text(line, x, cy, size, color, font);
                cy += lineH;
                line = word;
            } else {
                line = tryLine;
            }
        }
        if (!line.empty()) {
            if (draw) ui_text(line, x, cy, size, color, font);
            cy += lineH;
        }
    }
    return cy - y;
}

void ui_text_centered_wrapped(const std::string &text, float cx, float y, float width, float size,
                              Color color, int maxLines, FontKind font)
{
    std::vector<std::string> lines;
    std::stringstream words(text);
    std::string word, line;
    while (words >> word) {
        std::string tryLine = line.empty() ? word : line + " " + word;
        if (!line.empty() && ui_text_width(tryLine, size, font) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = tryLine;
        }
    }
    if (!line.empty()) lines.push_back(line);

    for (int i = 0; i < (int)lines.size() && i < maxLines; i++) {
        float w = ui_text_width(lines[i], size, font);
        ui_text(lines[i], cx - w / 2, y + i * size * 1.3f, size, color, font);
    }
}

// ---------------------------------------------------------------------------
// Shapes, panels and buttons
// ---------------------------------------------------------------------------

// Rounded corners scaled to the box, so small and large boxes look the same.
static float roundness_for(Rectangle r, float radius)
{
    float shortSide = r.width < r.height ? r.width : r.height;
    if (shortSide <= 0) return 0;
    float k = (2 * radius) / shortSide;
    return k > 1 ? 1 : k;
}

// The border is a slightly bigger box drawn underneath. This works the same
// on every raylib version (DrawRectangleRoundedLines changed between 5.0 and 5.5).
void ui_panel(Rectangle r, Color fill, Color border, float borderWidth)
{
    if (borderWidth > 0) DrawRectangleRounded(r, roundness_for(r, 8), 8, border);
    Rectangle inner = { r.x + borderWidth, r.y + borderWidth, r.width - 2 * borderWidth, r.height - 2 * borderWidth };
    DrawRectangleRounded(inner, roundness_for(inner, 8 - borderWidth), 8, fill);
}

void ui_triangle(Vector2 a, Vector2 b, Vector2 c, Color color)
{
    // raylib only draws triangles given counter-clockwise; one of these two is.
    DrawTriangle(a, b, c, color);
    DrawTriangle(a, c, b, color);
}

// A colour part way from the background to c (solid, so nothing shows through).
static Color mix_bg(Color c, float amount)
{
    return { (unsigned char)(COL_BG.r + (c.r - COL_BG.r) * amount), (unsigned char)(COL_BG.g + (c.g - COL_BG.g) * amount),
             (unsigned char)(COL_BG.b + (c.b - COL_BG.b) * amount), 255 };
}

// Buttons are chunky, Duolingo style: a raised face on a darker base. Holding
// the mouse down pushes the face down onto the base, and letting go clicks.
const float BUTTON_DEPTH = 4;

bool ui_button(const std::string &label, Rectangle r, ButtonStyle style, Color accent)
{
    bool hover = style != ButtonStyle::Disabled && CheckCollisionPointRec(GetMousePosition(), r);
    if (hover) wantHand = true;
    bool held = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    Color fill, border, textColor;
    switch (style) {
        case ButtonStyle::Primary:
            fill = hover ? ColorBrightness(accent, 0.15f) : accent;
            border = fill;
            textColor = COL_BG;
            break;
        case ButtonStyle::Outline:
            fill = hover ? COL_SURFACE_2 : COL_BG;
            border = accent;
            textColor = accent;
            break;
        default:   // Disabled
            fill = COL_BG;
            border = COL_LINE_SOFT;
            textColor = COL_MUTED;
            break;
    }
    // The base, then the face: up by the depth normally, nearly flat when held
    Color base = style == ButtonStyle::Primary ? mix_bg(accent, 0.55f)
               : style == ButtonStyle::Outline ? mix_bg(accent, 0.4f) : COL_LINE_SOFT;
    ui_panel({ r.x, r.y + BUTTON_DEPTH, r.width, r.height - BUTTON_DEPTH }, base, base, 0);
    float sink = held ? BUTTON_DEPTH - 1 : 0;
    Rectangle face = { r.x, r.y + sink, r.width, r.height - BUTTON_DEPTH };
    ui_panel(face, fill, border, style == ButtonStyle::Outline ? 1.5f : 1);

    float size = 16;
    float w = ui_text_width(label, size, FontKind::UiBold);
    ui_text(label, face.x + (face.width - w) / 2, face.y + (face.height - size * 1.25f) / 2, size, textColor, FontKind::UiBold);

    bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (clicked) sfx_play(Sfx::Click);
    return clicked;
}

// ---------------------------------------------------------------------------
// Header and footer
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// The logo
// ---------------------------------------------------------------------------

// A rounded box with an outline: drawn as the outline colour, then the fill
// inset by the line's thickness (on our solid backgrounds that's the same).
static void logo_box(Rectangle r, float radius, float line, Color fill, Color outline)
{
    auto roundness = [](Rectangle b, float rad) {
        float shorter = b.width < b.height ? b.width : b.height;
        return shorter > 0 ? (2 * rad / shorter > 1 ? 1 : 2 * rad / shorter) : 0;
    };
    if (line > 0) {
        DrawRectangleRounded(r, roundness(r, radius), 8, outline);
        r = { r.x + line, r.y + line, r.width - 2 * line, r.height - 2 * line };
        radius -= line;
    }
    DrawRectangleRounded(r, roundness(r, radius), 8, fill);
}

// An arrowhead: two short lines meeting at tip, pointing along (dx, dy).
static void logo_arrowhead(Vector2 tip, float dx, float dy, float len, float thick, Color c)
{
    float l = sqrtf(dx * dx + dy * dy);
    dx /= l; dy /= l;
    Vector2 a = { tip.x - dx * len - dy * len * 0.75f, tip.y - dy * len + dx * len * 0.75f };
    Vector2 b = { tip.x - dx * len + dy * len * 0.75f, tip.y - dy * len - dx * len * 0.75f };
    DrawLineEx(a, tip, thick, c);
    DrawLineEx(b, tip, thick, c);
    DrawCircleV(tip, thick / 2, c);
}

void ui_logo(float x, float y, float size)
{
    // Laid out on a 170 x 170 grid, then scaled.
    float k = size / 170.0f;
    auto R = [&](float rx, float ry, float rw, float rh) { return Rectangle{ x + rx * k, y + ry * k, rw * k, rh * k }; };
    auto P = [&](float px, float py) { return Vector2{ x + px * k, y + py * k }; };
    float line = fmaxf(1.0f, 2.5f * k);
    bool detailed = size >= 100;

    logo_box(R(0, 0, 170, 170), 28 * k, fmaxf(1.0f, 1.2f * k), COL_BG, COL_LINE);

    // The stack: the newest frame on top, in amber
    logo_box(R(88, 32, 66, 24), 5 * k, 0, COL_AMBER, COL_AMBER);
    logo_box(R(88, 64, 66, 24), 5 * k, line * 0.8f, COL_SURFACE, COL_PURPLE);
    logo_box(R(88, 96, 66, 24), 5 * k, line * 0.8f, COL_SURFACE, COL_LINE);
    logo_box(R(88, 128, 66, 22), 5 * k, line * 0.8f, COL_SURFACE, COL_LINE);
    if (detailed) {
        float fs = 13 * k;
        ui_text("swap", x + 121 * k - ui_text_width("swap", fs, FontKind::MonoBold) / 2, y + 37 * k, fs, COL_BG, FontKind::MonoBold);
        ui_text("main", x + 121 * k - ui_text_width("main", fs, FontKind::Mono) / 2, y + 69 * k, fs, COL_PURPLE, FontKind::Mono);
    }

    // The trace line, climbing from the oldest frame to p's frame
    float tl = 3.5f * k;
    Vector2 trace[] = { P(18, 139), P(42, 139), P(56, 108), P(68, 76), P(82, 76) };
    for (int i = 0; i < 4; i++) {
        DrawLineEx(trace[i], trace[i + 1], tl, COL_GREEN);
        DrawCircleV(trace[i + 1], tl / 2, COL_GREEN);
    }
    DrawCircleV(trace[0], 4.5f * k, COL_GREEN);

    // The pointer p, with the green cursor from STACK_TRACE, and its arrow
    logo_box(R(16, 30, 44, 38), 7 * k, line, COL_SURFACE, COL_BLUE);
    float ps = 22 * k;
    ui_text("p", x + 38 * k - ui_text_width("p", ps, FontKind::MonoBold) / 2, y + 34 * k, ps, COL_BLUE, FontKind::MonoBold);
    DrawRectangleRec(R(44, 58, 10, 3.5f), COL_GREEN);
    DrawSplineSegmentBezierCubic(P(60, 49), P(72, 49), P(74, 44), P(83, 44), 3 * k, COL_BLUE);
    logo_arrowhead(P(84, 44), 1, 0, 7 * k, 3 * k, COL_BLUE);
}

void ui_header(const std::string &subtitle)
{
    DrawRectangle(0, 0, SCREEN_W, HEADER_H, COL_BG);
    DrawLine(0, HEADER_H - 1, SCREEN_W, HEADER_H - 1, COL_LINE_SOFT);

    ui_logo(MARGIN, 12, 40);
    float x = MARGIN + 52, y = 20;
    ui_text("STACK", x, y, 22, COL_TEXT, FontKind::MonoBold);
    x += ui_text_width("STACK", 22, FontKind::MonoBold);
    ui_text("_", x, y, 22, COL_GREEN, FontKind::MonoBold);
    x += ui_text_width("_", 22, FontKind::MonoBold);
    ui_text("TRACE", x, y, 22, COL_TEXT, FontKind::MonoBold);
    x += ui_text_width("TRACE", 22, FontKind::MonoBold) + 20;
    ui_text(subtitle, x, y + 4, 15, COL_MUTED);
}

void ui_footer_hints(const std::vector<std::string> &hints)
{
    float y = SCREEN_H - FOOTER_H;
    DrawLine(0, (int)y, SCREEN_W, (int)y, COL_LINE_SOFT);
    float x = MARGIN;
    for (const std::string &h : hints) {
        ui_text(h, x, y + 20, 13, COL_MUTED, FontKind::Mono);
        x += ui_text_width(h, 13, FontKind::Mono) + 28;
    }
}

// ---------------------------------------------------------------------------
// Code listings
// ---------------------------------------------------------------------------

int ui_code(const std::vector<std::string> &lines, Rectangle r, int highlight, Color highlightColor,
            bool clickable, float size)
{
    ui_panel(r, COL_SURFACE, COL_LINE, 1);
    float lineH = size * 1.55f;
    float top = r.y + 10;
    int clicked = 0;

    ui_scissor((int)r.x, (int)r.y, (int)r.width, (int)r.height);
    for (int i = 0; i < (int)lines.size(); i++) {
        int number = i + 1;
        Rectangle row = { r.x + 1, top + i * lineH, r.width - 2, lineH };

        bool hover = clickable && CheckCollisionPointRec(GetMousePosition(), row);
        if (hover) {
            wantHand = true;
            DrawRectangleRec(row, Fade(COL_TEXT, 0.06f));
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = number;
        }
        if (number == highlight) {
            DrawRectangleRec(row, Fade(highlightColor, 0.15f));
            DrawRectangle((int)row.x, (int)row.y, 3, (int)row.height, highlightColor);
        }

        std::string num = std::to_string(number);
        float numW = ui_text_width(num, size, FontKind::Mono);
        float textY = row.y + (lineH - size) / 2;
        ui_text(num, r.x + 36 - numW, textY, size, number == highlight ? highlightColor : COL_MUTED, FontKind::Mono);
        ui_text(lines[i], r.x + 52, textY, size, number == highlight ? COL_TEXT : COL_TEXT_SOFT, FontKind::Mono);
    }
    EndScissorMode();
    return clicked;
}

// ---------------------------------------------------------------------------
// Hearts and mastery colours
// ---------------------------------------------------------------------------

static void draw_heart(float x, float y, float s, Color color)
{
    // Two circles and a triangle make a heart s pixels wide.
    float r = s / 4;
    DrawCircleV({ x + r, y + r }, r, color);
    DrawCircleV({ x + 3 * r, y + r }, r, color);
    ui_triangle({ x, y + r * 1.2f }, { x + s, y + r * 1.2f }, { x + s / 2, y + s * 0.95f }, color);
}

float ui_level_badge_width(int xp)
{
    int level = level_for_xp(xp);
    std::string lv = "LV " + std::to_string(level);
    float boxW = ui_text_width(lv, 14, FontKind::MonoBold) + 18;
    float titleW = ui_text_width(level_title(level), 13, FontKind::UiBold);
    return boxW + 10 + fmaxf(titleW + 56, 150);
}

float ui_level_badge(float x, float y, int xp)
{
    int level = level_for_xp(xp);
    int from = xp_for_level(level), to = xp_for_level(level + 1);
    std::string lv = "LV " + std::to_string(level);
    float boxW = ui_text_width(lv, 14, FontKind::MonoBold) + 18;

    ui_panel({ x, y + 3, boxW, 34 }, mix_bg(COL_AMBER, 0.55f), mix_bg(COL_AMBER, 0.55f), 0);   // chunky, like the buttons
    ui_panel({ x, y, boxW, 34 }, COL_AMBER, COL_AMBER, 0);
    ui_text(lv, x + 9, y + 8, 14, COL_BG, FontKind::MonoBold);

    float tx = x + boxW + 10, w = ui_level_badge_width(xp) - boxW - 10;
    ui_text(level_title(level), tx, y + 1, 13, COL_TEXT, FontKind::UiBold);
    Rectangle bar = { tx, y + 22, w, 6 };
    DrawRectangleRounded(bar, 1, 6, COL_LINE_SOFT);
    float f = to > from ? (float)(xp - from) / (to - from) : 0;
    if (f > 0) DrawRectangleRounded({ bar.x, bar.y, bar.width * f, bar.height }, 1, 6, COL_AMBER);
    std::string count = std::to_string(xp - from) + "/" + std::to_string(to - from);
    ui_text(count, tx + w - ui_text_width(count, 10, FontKind::Mono), y + 3, 10, COL_MUTED, FontKind::Mono);
    return boxW + 10 + w;
}

void ui_hearts(float x, float y, int lives, int maxLives)
{
    for (int i = 0; i < maxLives; i++)
        draw_heart(x + i * 22, y, 16, i < lives ? COL_CORAL : COL_LINE);
}

Color mastery_color(Mastery m)
{
    switch (m) {
        case Mastery::Mastered:   return COL_GREEN;
        case Mastery::Proficient: return COL_BLUE;
        case Mastery::Familiar:   return COL_AMBER;
        case Mastery::Attempted:  return COL_CORAL;
        default:                  return COL_LINE;
    }
}
