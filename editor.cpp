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
        if (cmd && IsKeyPressed(KEY_X)) {   // cut: the selection, or the whole line (like VS Code)
            if (eb_has_selection(b)) {
                SetClipboardText(eb_selected_text(b).c_str());
                eb_delete_selection(b);
            } else {
                SetClipboardText((b.lines[b.row] + "\n").c_str());
                int r = b.row;
                if (r + 1 < (int)b.lines.size()) { b.selRow = r; b.selCol = 0; eb_move_to(b, r + 1, 0); }
                else if (r > 0) { b.selRow = r - 1; b.selCol = (int)b.lines[r - 1].size(); eb_move_to(b, r, (int)b.lines[r].size()); }
                else { b.selRow = r; b.selCol = 0; eb_move_to(b, r, (int)b.lines[r].size()); }
                eb_delete_selection(b);
            }
            input = true;
        }
        if (cmd && key_repeat(KEY_Z)) { eb_undo(b); input = true; }
    }
    if (focused) {
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool cmd = command_down();
        // Copy: the selection, or the whole line when nothing's selected (works read-only too)
        if (cmd && IsKeyPressed(KEY_C)) {
            std::string text = eb_has_selection(b) ? eb_selected_text(b) : b.lines[b.row] + "\n";
            SetClipboardText(text.c_str());
            v.copiedAt = GetTime();
        }
        if (cmd && IsKeyPressed(KEY_A)) { eb_select_all(b); input = true; }
        // Moving: with Shift it selects, without it the selection goes
        auto move = [&](void (*f)(EditBuffer &), int times) {
            if (shift) eb_start_selection(b);
            else eb_clear_selection(b);
            for (int i = 0; i < times; i++) f(b);
            input = true;
        };
        if (key_repeat(KEY_LEFT)) move(eb_left, 1);
        if (key_repeat(KEY_RIGHT)) move(eb_right, 1);
        if (key_repeat(KEY_UP)) move(eb_up, 1);
        if (key_repeat(KEY_DOWN)) move(eb_down, 1);
        if (IsKeyPressed(KEY_HOME)) move(eb_home, 1);
        if (IsKeyPressed(KEY_END)) move(eb_end, 1);
        if (key_repeat(KEY_PAGE_UP)) move(eb_up, visibleRows - 1);
        if (key_repeat(KEY_PAGE_DOWN)) move(eb_down, visibleRows - 1);
    }

    // Jump to a line (clicking a problem in the Problems list)
    if (v.jumpRow >= 0) {
        eb_move_to(b, v.jumpRow, v.jumpCol);
        v.jumpRow = -1;
        input = true;
        if (b.row >= visibleRows / 2) v.scrollRow = b.row - visibleRows / 2;   // put it in the middle
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
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            if (shift) eb_start_selection(b);   // Shift+click selects up to there
            eb_move_to(b, row, col);
            if (!shift) { b.selRow = b.row; b.selCol = b.col; }   // dragging from here selects
            v.dragging = true;
            input = true;
        }
    }
    // Dragging with the button held selects (it scrolls when the mouse goes past the edge)
    if (v.dragging) {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            v.dragging = false;
            if (!eb_has_selection(b)) eb_clear_selection(b);
        } else {
            int row = v.scrollRow + (int)floorf((mouse.y - text.y) / LINE_H);
            int col = v.scrollCol + (int)std::lround((mouse.x - text.x) / cw);
            if (row != b.row || col != b.col) { eb_move_to(b, row, col); input = true; }
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
    ui_scissor((int)r.x, (int)r.y, (int)r.width, (int)r.height);
    for (int i = 0; i < visibleRows + 1; i++) {
        int row = v.scrollRow + i;
        if (row >= (int)b.lines.size()) break;
        float y = text.y + i * LINE_H;

        if (row == b.row && focused && !eb_has_selection(b)) DrawRectangle((int)text.x - 6, (int)y, (int)text.width + 6, (int)LINE_H, COL_SURFACE);
        int sf, st;
        eb_selection_on_row(b, row, sf, st);
        if (st > sf) {   // the selected part of this line
            float x0 = text.x + (sf - v.scrollCol) * cw, x1 = text.x + (st - v.scrollCol) * cw;
            if (x0 < text.x) x0 = text.x;
            if (x1 > x0) DrawRectangle((int)x0, (int)y, (int)(x1 - x0), (int)LINE_H, Fade(COL_BLUE, 0.28f));
        }
        const EditorMark *mark = nullptr;
        for (const EditorMark &m : v.marks)
            if (m.row == row && m.lineText == b.lines[row] && (!mark || (m.error && !mark->error))) mark = &m;
        Color mc = mark && mark->error ? COL_CORAL : COL_AMBER;
        if (mark) DrawRectangle((int)text.x - 6, (int)y, (int)text.width + 6, (int)LINE_H, Fade(mc, 0.12f));
        std::string num = std::to_string(row + 1);
        float nw = ui_text_width(num, 13, FontKind::Mono);
        ui_text(num, r.x + GUTTER - 16 - nw, y + 4, 13, mark ? mc : row == b.row ? COL_TEXT_SOFT : COL_MUTED, FontKind::Mono);
        editor_draw_code_line(b.lines[row], text.x, y + 3, CODE_SIZE, v.scrollCol, visibleCols + 1);
        if (mark) {
            DrawCircleV({ r.x + 8, y + LINE_H / 2 }, 4, mc);   // a dot in the margin
            // a wavy line under the code, then the message after it
            int first = 0, last = (int)b.lines[row].size();
            while (first < last && b.lines[row][first] == ' ') first++;
            float x0 = text.x + (first - v.scrollCol) * cw, x1 = text.x + (last - v.scrollCol) * cw;
            if (x1 <= x0) x1 = x0 + cw * 3;
            float uy = y + LINE_H - 3;
            for (float x = fmaxf(x0, text.x); x < x1; x += 4)
                DrawLineEx({ x, uy + (((int)(x / 4)) % 2 ? 1.5f : -1.5f) }, { x + 4, uy + (((int)(x / 4)) % 2 ? -1.5f : 1.5f) }, 1.2f, mc);
            float mx = x1 + cw * 3;
            if (mx < text.x + text.width - 60) ui_text(mark->message, mx, y + 5, 12, Fade(mc, 0.85f), FontKind::Mono);
        }
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
    if (GetTime() - v.copiedAt < 1.2) {   // a little "Copied" in the corner
        const char *msg = "Copied";
        float mw = ui_text_width(msg, 12, FontKind::MonoBold) + 16;
        Rectangle tag = { r.x + r.width - mw - 10, r.y + 8, mw, 22 };
        DrawRectangleRounded(tag, 0.4f, 6, COL_GREEN);
        ui_text(msg, tag.x + 8, tag.y + 4, 12, COL_BG, FontKind::MonoBold);
    }
}
