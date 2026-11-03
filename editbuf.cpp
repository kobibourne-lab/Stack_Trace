// editbuf.cpp - the editor's text and edits. See editbuf.h.

#include "editbuf.h"
#include <algorithm>
#include <cctype>
#include <sstream>

const size_t MAX_UNDO = 200;

static int leading_spaces(const std::string &s)
{
    int n = 0;
    while (n < (int)s.size() && s[n] == ' ') n++;
    return n;
}

// Saved before every change, so undo can go back to it.
static void snapshot(EditBuffer &b)
{
    b.undo.push_back({ b.lines, b.row, b.col });
    if (b.undo.size() > MAX_UNDO) b.undo.erase(b.undo.begin());
}

static void edited(EditBuffer &b)
{
    b.changed = true;
    b.wantCol = b.col;
}

void eb_set_text(EditBuffer &b, const std::string &text)
{
    b.lines.clear();
    std::string line;
    std::stringstream ss(text);
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string clean;
        for (char c : line) {
            if (c == '\t') clean += std::string(INDENT, ' ');
            else clean += c;
        }
        b.lines.push_back(clean);
    }
    if (b.lines.empty()) b.lines.push_back("");
    b.row = b.col = b.wantCol = 0;
    b.undo.clear();
    b.changed = false;
    b.lastWasTyping = false;
}

void eb_replace_all(EditBuffer &b, const std::string &text)
{
    std::vector<EditSnapshot> undo = b.undo;
    undo.push_back({ b.lines, b.row, b.col });
    eb_set_text(b, text);
    b.undo = undo;
    b.changed = true;
}

std::string eb_text(const EditBuffer &b)
{
    std::string s;
    for (size_t i = 0; i < b.lines.size(); i++) {
        s += b.lines[i];
        s += "\n";
    }
    return s;
}

void eb_type(EditBuffer &b, char c)
{
    // A run of letters is one undo step; a space or symbol starts a new one
    bool letter = isalnum((unsigned char)c) || c == '_';
    if (!(b.lastWasTyping && letter)) snapshot(b);
    b.lastWasTyping = letter;

    std::string &line = b.lines[b.row];
    // '}' typed on a line of only spaces moves back one indent level
    if (c == '}' && leading_spaces(line) == (int)line.size() && b.col == (int)line.size() && b.col >= INDENT) {
        line.erase(0, INDENT);
        b.col -= INDENT;
    }
    line.insert(line.begin() + b.col, c);
    b.col++;
    edited(b);
}

void eb_newline(EditBuffer &b)
{
    snapshot(b);
    b.lastWasTyping = false;
    std::string &line = b.lines[b.row];
    std::string before = line.substr(0, b.col), after = line.substr(b.col);

    int indent = leading_spaces(before);
    if (indent > (int)before.size()) indent = (int)before.size();
    std::string trimmedBefore = before;
    while (!trimmedBefore.empty() && trimmedBefore.back() == ' ') trimmedBefore.pop_back();
    bool opens = !trimmedBefore.empty() && trimmedBefore.back() == '{';
    int newIndent = indent + (opens ? INDENT : 0);

    // Strip leading spaces from what moves down, so it lines up with the new indent
    size_t start = after.find_first_not_of(' ');
    after = (start == std::string::npos) ? "" : after.substr(start);

    line = before;
    if (opens && !after.empty() && after[0] == '}') {
        // Enter between { and }: the cursor goes on its own indented line, } below it
        b.lines.insert(b.lines.begin() + b.row + 1, std::string(newIndent, ' '));
        b.lines.insert(b.lines.begin() + b.row + 2, std::string(indent, ' ') + after);
    } else {
        b.lines.insert(b.lines.begin() + b.row + 1, std::string(newIndent, ' ') + after);
    }
    b.row++;
    b.col = newIndent;
    edited(b);
}

void eb_backspace(EditBuffer &b)
{
    if (b.row == 0 && b.col == 0) return;
    snapshot(b);
    b.lastWasTyping = false;

    if (b.col == 0) {
        // Join this line onto the one above
        int prevLen = (int)b.lines[b.row - 1].size();
        b.lines[b.row - 1] += b.lines[b.row];
        b.lines.erase(b.lines.begin() + b.row);
        b.row--;
        b.col = prevLen;
    } else {
        std::string &line = b.lines[b.row];
        int remove = 1;
        if (b.col <= leading_spaces(line)) {
            // In the indent: remove back to the previous indent level
            remove = b.col % INDENT;
            if (remove == 0) remove = INDENT;
        }
        line.erase(b.col - remove, remove);
        b.col -= remove;
    }
    edited(b);
}

void eb_delete(EditBuffer &b)
{
    std::string &line = b.lines[b.row];
    if (b.col >= (int)line.size() && b.row + 1 >= (int)b.lines.size()) return;
    snapshot(b);
    b.lastWasTyping = false;
    if (b.col < (int)line.size()) {
        line.erase(b.col, 1);
    } else {
        line += b.lines[b.row + 1];
        b.lines.erase(b.lines.begin() + b.row + 1);
    }
    edited(b);
}

void eb_tab(EditBuffer &b)
{
    snapshot(b);
    b.lastWasTyping = false;
    int spaces = INDENT - (b.col % INDENT);
    b.lines[b.row].insert(b.col, std::string(spaces, ' '));
    b.col += spaces;
    edited(b);
}

void eb_paste(EditBuffer &b, const std::string &text)
{
    if (text.empty()) return;
    snapshot(b);
    b.lastWasTyping = false;
    std::string &line = b.lines[b.row];
    std::string after = line.substr(b.col);
    line = line.substr(0, b.col);

    std::vector<std::string> parts;
    std::string part;
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n') { parts.push_back(part); part.clear(); }
        else if (c == '\t') part += std::string(INDENT, ' ');
        else if ((unsigned char)c >= 32 && (unsigned char)c < 127) part += c;
    }
    parts.push_back(part);

    b.lines[b.row] += parts[0];
    for (size_t i = 1; i < parts.size(); i++) b.lines.insert(b.lines.begin() + b.row + i, parts[i]);
    b.row += (int)parts.size() - 1;
    b.col = (int)b.lines[b.row].size();
    b.lines[b.row] += after;
    edited(b);
}

// ---------------------------------------------------------------------------
// Moving the cursor
// ---------------------------------------------------------------------------

static void moved(EditBuffer &b) { b.lastWasTyping = false; }

void eb_left(EditBuffer &b)
{
    if (b.col > 0) b.col--;
    else if (b.row > 0) { b.row--; b.col = (int)b.lines[b.row].size(); }
    b.wantCol = b.col;
    moved(b);
}

void eb_right(EditBuffer &b)
{
    if (b.col < (int)b.lines[b.row].size()) b.col++;
    else if (b.row + 1 < (int)b.lines.size()) { b.row++; b.col = 0; }
    b.wantCol = b.col;
    moved(b);
}

void eb_up(EditBuffer &b)
{
    if (b.row > 0) b.row--;
    else b.wantCol = 0;
    b.col = std::min(b.wantCol, (int)b.lines[b.row].size());
    moved(b);
}

void eb_down(EditBuffer &b)
{
    if (b.row + 1 < (int)b.lines.size()) b.row++;
    else b.wantCol = (int)b.lines[b.row].size();
    b.col = std::min(b.wantCol, (int)b.lines[b.row].size());
    moved(b);
}

void eb_home(EditBuffer &b)
{
    int first = leading_spaces(b.lines[b.row]);
    b.col = (b.col == first) ? 0 : first;
    b.wantCol = b.col;
    moved(b);
}

void eb_end(EditBuffer &b)
{
    b.col = (int)b.lines[b.row].size();
    b.wantCol = b.col;
    moved(b);
}

void eb_move_to(EditBuffer &b, int row, int col)
{
    if (row < 0) row = 0;
    if (row >= (int)b.lines.size()) row = (int)b.lines.size() - 1;
    if (col < 0) col = 0;
    if (col > (int)b.lines[row].size()) col = (int)b.lines[row].size();
    b.row = row;
    b.col = col;
    b.wantCol = col;
    moved(b);
}

void eb_undo(EditBuffer &b)
{
    if (b.undo.empty()) return;
    EditSnapshot s = b.undo.back();
    b.undo.pop_back();
    b.lines = s.lines;
    b.row = s.row;
    b.col = s.col;
    b.wantCol = b.col;
    b.changed = true;
    b.lastWasTyping = false;
}
