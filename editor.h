// editor.h - the code editor on the problem screen: draws an EditBuffer with
// line numbers and C/C++ colours, and turns keys and clicks into edits.
//
// Keys: type as normal, Enter keeps the indent, Tab indents, arrows / Home /
// End / Page Up / Page Down move, Ctrl+V (Cmd+V on Mac) pastes, Ctrl+Z undoes.

#pragma once
#include "raylib.h"
#include "editbuf.h"

struct EditorView {
    int scrollRow = 0, scrollCol = 0;  // the first line and column shown
    double lastInput = 0;              // the cursor stays solid just after typing
};

// Handles input (when focused and not read-only) and draws the editor in r.
void editor_update_draw(EditBuffer &b, EditorView &v, Rectangle r, bool focused, bool readOnly);

// Draws one line of C/C++ with colours (also used for read-only code elsewhere).
void editor_draw_code_line(const std::string &line, float x, float y, float size, int firstCol, int maxCols);
