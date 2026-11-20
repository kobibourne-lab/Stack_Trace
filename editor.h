// editor.h - the code editor on the problem screen: draws an EditBuffer with
// line numbers and C/C++ colours, and turns keys and clicks into edits.
//
// Keys: type as normal, Enter keeps the indent, Tab indents, arrows / Home /
// End / Page Up / Page Down move (with Shift they select), Ctrl+A selects all,
// Ctrl+C copies, Ctrl+X cuts, Ctrl+V pastes, Ctrl+Z undoes (Cmd on a Mac).
// Click and drag selects; with nothing selected, copy and cut take the line.

#pragma once
#include <string>
#include <vector>
#include "raylib.h"
#include "editbuf.h"

// A compiler error or warning on a line, drawn VS Code style: the line tinted,
// a wavy underline, a dot in the margin and the message after the code. It
// disappears once the line is changed.
struct EditorMark {
    int row = 0;               // 0-based
    bool error = true;         // false: a warning (amber)
    std::string message;
    std::string lineText;      // the line when it was marked
};

struct EditorView {
    int scrollRow = 0, scrollCol = 0;  // the first line and column shown
    double lastInput = 0;              // the cursor stays solid just after typing
    std::vector<EditorMark> marks;
    int jumpRow = -1, jumpCol = 0;     // set to move the cursor there (and scroll to it) next frame
    bool dragging = false;             // selecting with the mouse
    double copiedAt = -10;             // when Ctrl+C copied (shows "Copied")
};

// Handles input (when focused and not read-only) and draws the editor in r.
void editor_update_draw(EditBuffer &b, EditorView &v, Rectangle r, bool focused, bool readOnly);

// Draws one line of C/C++ with colours (also used for read-only code elsewhere).
void editor_draw_code_line(const std::string &line, float x, float y, float size, int firstCol, int maxCols);
