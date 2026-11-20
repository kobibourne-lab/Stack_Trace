// editbuf.h - the text inside the code editor, and the edits that change it:
// typing, new lines (with auto-indent), backspace, moving the cursor, paste
// and undo. No drawing in here; editor.cpp draws it and turns key presses
// into these calls. Being separate, it's tested in tests/test_core.cpp.

#pragma once
#include <string>
#include <vector>

const int INDENT = 4;   // spaces per indent level (Tab inserts up to the next multiple)

struct EditSnapshot {
    std::vector<std::string> lines;
    int row, col;
};

struct EditBuffer {
    std::vector<std::string> lines = { "" };
    int row = 0, col = 0;       // the cursor (col can equal the line's length: after the last char)
    int wantCol = 0;            // the column up/down tries to keep
    bool changed = false;       // edited since the last save
    std::vector<EditSnapshot> undo;
    bool lastWasTyping = false; // typing several letters is one undo step
    int selRow = -1, selCol = 0;  // the selection's other end (selRow -1: nothing selected)
};

void eb_set_text(EditBuffer &b, const std::string &text);   // also clears undo
void eb_replace_all(EditBuffer &b, const std::string &text); // can be undone (Reset code)
std::string eb_text(const EditBuffer &b);

void eb_type(EditBuffer &b, char c);            // one printable character ('}' on a blank line un-indents)
void eb_newline(EditBuffer &b);                  // keeps the indent, adds one after '{'
void eb_backspace(EditBuffer &b);                // in leading spaces, removes a whole indent
void eb_delete(EditBuffer &b);
void eb_tab(EditBuffer &b);
void eb_paste(EditBuffer &b, const std::string &text);

void eb_left(EditBuffer &b);
void eb_right(EditBuffer &b);
void eb_up(EditBuffer &b);
void eb_down(EditBuffer &b);
void eb_home(EditBuffer &b);                     // to the first non-space, then to column 0
void eb_end(EditBuffer &b);
void eb_move_to(EditBuffer &b, int row, int col); // clamped, e.g. from a mouse click

void eb_undo(EditBuffer &b);

// Selecting: the text between the anchor (selRow, selCol) and the cursor.
// Typing, Enter, paste, backspace and delete replace what's selected.
bool eb_has_selection(const EditBuffer &b);
void eb_start_selection(EditBuffer &b);          // anchor at the cursor, if there isn't one already
void eb_clear_selection(EditBuffer &b);
void eb_select_all(EditBuffer &b);
std::string eb_selected_text(const EditBuffer &b);
bool eb_delete_selection(EditBuffer &b);         // false if nothing was selected
// The selected columns on a row: from <= col < to (from == to: none on that row)
void eb_selection_on_row(const EditBuffer &b, int row, int &from, int &to);
