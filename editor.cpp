// editor.cpp - the code editor. See editor.h.

#include "editor.h"
#include "theme.h"
#include "ui.h"
#include <cmath>
#include <cctype>
#include <set>
#include <string>

const float CODE_SIZE = 15;
const float LINE_H = 22;
const float GUTTER = 48;

// ---------------------------------------------------------------------------
// Colours
// ---------------------------------------------------------------------------

static const std::set<std::string> CONTROL = {
    "if", "else", "for", "while", "do", "return", "switch", "case", "break", "continue", "default", "new", "delete",
};
static const std::set<std::string> TYPES = {
    "int", "char", "bool", "void", "float", "double", "long", "short", "unsigned", "signed", "const", "auto",
    "class", "struct", "public", "private", "protected", "static", "nullptr", "NULL", "true", "false",
    "using", "namespace", "std", "this", "sizeof", "size_t", "string",
};

static float char_width()
{
    static float w = 0;
    if (w == 0) w = ui_text_width("MMMMMMMMMM", CODE_SIZE, FontKind::Mono) / 10.0f;
    return w;
}

void editor_draw_code_line(const std::string &line, float x, float y, float size, int firstCol, int maxCols)
{
    float cw = ui_text_width("MMMMMMMMMM", size, FontKind::Mono) / 10.0f;
    size_t i = 0, n = line.size();

    // Draws one token, skipping the part scrolled off the left and right
    auto draw = [&](size_t start, size_t end, Color c) {
        for (size_t k = start; k < end; k++) {
            int col = (int)k - firstCol;
            if (col < 0 || col >= maxCols) continue;
            if (line[k] == ' ') continue;
            char s[2] = { line[k], 0 };
            ui_text(s, x + col * cw, y, size, c, FontKind::Mono);
        }
    };

    size_t firstNonSpace = line.find_first_not_of(' ');
    if (firstNonSpace != std::string::npos && line[firstNonSpace] == '#') {
        draw(0, n, COL_PURPLE);
        return;
    }
    while (i < n) {
        char c = line[i];
        if (c == '/' && i + 1 < n && line[i + 1] == '/') {
            draw(i, n, COL_MUTED);
            return;
        } else if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < n && line[j] != c) { if (line[j] == '\\') j++; j++; }
            j = std::min(j + 1, n);
            draw(i, j, COL_GREEN);
            i = j;
        } else if (isdigit((unsigned char)c)) {
            size_t j = i;
            while (j < n && (isalnum((unsigned char)line[j]) || line[j] == '.')) j++;
            draw(i, j, COL_AMBER);
            i = j;
        } else if (isalpha((unsigned char)c) || c == '_') {
            size_t j = i;
            while (j < n && (isalnum((unsigned char)line[j]) || line[j] == '_')) j++;
            std::string word = line.substr(i, j - i);
            Color col = CONTROL.count(word) ? COL_PURPLE : (TYPES.count(word) ? COL_BLUE : COL_TEXT);
            draw(i, j, col);
            i = j;
        } else {
            draw(i, i + 1, COL_TEXT_SOFT);
            i++;
        }
    }
}

// ---------------------------------------------------------------------------
// Keys that repeat while held (works the same on every raylib version)
// ---------------------------------------------------------------------------

static bool key_repeat(int key)
{
    static double downSince[512] = { 0 };
    static double lastRepeat[512] = { 0 };
    double now = GetTime();
    if (IsKeyPressed(key)) {
        downSince[key] = now;
        lastRepeat[key] = now;
        return true;
    }
    if (IsKeyDown(key) && now - downSince[key] > 0.45 && now - lastRepeat[key] > 0.035) {
        lastRepeat[key] = now;
        return true;
    }
    return false;
}

static bool command_down()
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}

// ---------------------------------------------------------------------------

void editor_update_draw(EditBuffer &b, EditorView &v, Rectangle r, bool focused, bool readOnly)
{
    float cw = char_width();
    Rectangle text = { r.x + GUTTER, r.y + 8, r.width - GUTTER - 12, r.height - 16 };
    int visibleRows = (int)(text.height / LINE_H);
    int visibleCols = (int)(text.width / cw);
    bool input = false;

    // ----- Input -----
    if (focused && !readOnly) {
        bool cmd = command_down();
        int c;
        while ((c = GetCharPressed()) > 0) {
            if (!cmd && c >= 32 && c < 127) { eb_type(b, (char)c); input = true; }
        }
        if (key_repeat(KEY_ENTER) || key_repeat(KEY_KP_ENTER)) { if (!cmd) { eb_newline(b); input = true; } }
        if (key_repeat(KEY_BACKSPACE)) { eb_backspace(b); input = true; }
        if (key_repeat(KEY_DELETE)) { eb_delete(b); input = true; }
        if (IsKeyPressed(KEY_TAB)) { eb_tab(b); input = true; }
        if (cmd && IsKeyPressed(KEY_V)) {
            const char *clip = GetClipboardText();
            if (clip != nullptr) eb_paste(b, clip);
            input = true;
        }
        if (cmd && key_repeat(KEY_Z)) { eb_undo(b); input = true; }
    }
    if (focused) {
        if (key_repeat(KEY_LEFT)) { eb_left(b); input = true; }
        if (key_repeat(KEY_RIGHT)) { eb_right(b); input = true; }
        if (key_repeat(KEY_UP)) { eb_up(b); input = true; }
        if (key_repeat(KEY_DOWN)) { eb_down(b); input = true; }
        if (IsKeyPressed(KEY_HOME)) { eb_home(b); input = true; }
        if (IsKeyPressed(KEY_END)) { eb_end(b); input = true; }
        if (key_repeat(KEY_PAGE_UP)) { for (int i = 0; i < visibleRows - 1; i++) eb_up(b); input = true; }
        if (key_repeat(KEY_PAGE_DOWN)) { for (int i = 0; i < visibleRows - 1; i++) eb_down(b); input = true; }
    }

    // Mouse: click to place the cursor, wheel to scroll
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, r)) {
        if (CheckCollisionPointRec(mouse, text) && !readOnly) ui_want_ibeam();
        float wheel = GetMouseWheelMove();
        if (wheel != 0) v.scrollRow -= (int)(wheel * 3);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int row = v.scrollRow + (int)((mouse.y - text.y) / LINE_H);
            int col = v.scrollCol + (int)std::lround((mouse.x - text.x) / cw);
            eb_move_to(b, row, col);
            input = true;
        }
    }

    // Keep the cursor on screen after typing or moving
    if (input) {
        v.lastInput = GetTime();
        if (b.row < v.scrollRow) v.scrollRow = b.row;
        if (b.row >= v.scrollRow + visibleRows) v.scrollRow = b.row - visibleRows + 1;
        if (b.col < v.scrollCol) v.scrollCol = b.col;
        if (b.col >= v.scrollCol + visibleCols) v.scrollCol = b.col - visibleCols + 1;
    }
    int maxScroll = (int)b.lines.size() - visibleRows;
    if (v.scrollRow > maxScroll) v.scrollRow = maxScroll;
    if (v.scrollRow < 0) v.scrollRow = 0;

    // ----- Drawing -----
    DrawRectangleRec(r, COL_BG);
    DrawRectangle((int)r.x, (int)r.y, (int)GUTTER - 8, (int)r.height, COL_SURFACE);
    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
    for (int i = 0; i < visibleRows + 1; i++) {
        int row = v.scrollRow + i;
        if (row >= (int)b.lines.size()) break;
        float y = text.y + i * LINE_H;

        if (row == b.row && focused) DrawRectangle((int)text.x - 6, (int)y, (int)text.width + 6, (int)LINE_H, COL_SURFACE);
        std::string num = std::to_string(row + 1);
        float nw = ui_text_width(num, 13, FontKind::Mono);
        ui_text(num, r.x + GUTTER - 16 - nw, y + 4, 13, row == b.row ? COL_TEXT_SOFT : COL_MUTED, FontKind::Mono);
        editor_draw_code_line(b.lines[row], text.x, y + 3, CODE_SIZE, v.scrollCol, visibleCols + 1);
    }

    // The cursor: solid while typing, blinking otherwise
    if (focused && !readOnly) {
        double since = GetTime() - v.lastInput;
        bool on = since < 0.5 || std::fmod(since, 1.0) < 0.55;
        int screenRow = b.row - v.scrollRow, screenCol = b.col - v.scrollCol;
        if (on && screenRow >= 0 && screenRow < visibleRows + 1 && screenCol >= 0)
            DrawRectangle((int)(text.x + screenCol * cw), (int)(text.y + screenRow * LINE_H + 2), 2, (int)LINE_H - 4, COL_AMBER);
    }
    EndScissorMode();

    if (readOnly) DrawRectangleRec(r, Fade(COL_BG, 0.35f));
}
