// memview.cpp - the memory view. See memview.h.
//
// It works in two passes: first lay every cell out and remember where its
// value box is, then draw the boxes and the arrows between them.

#include "memview.h"
#include "theme.h"
#include "ui.h"
#include <map>

// Arrow colours, used in turn for each pointer on screen.
static const Color ARROW_COLORS[] = { COL_GREEN, COL_BLUE, COL_PURPLE, COL_AMBER };
static const int ARROW_COLOR_COUNT = 4;

const float ROW_H = 40;        // one variable in a stack frame
const float FRAME_HEAD = 30;   // the frame's name
const float FRAME_GAP = 14;
const float VALUE_W = 104;     // the box that holds the value
const float VALUE_H = 32;
const float ARROW_LANE = 12;   // space between side-by-side arrows

const float HEAP_HEAD = 18;    // a heap block's label above its box
const float HEAP_BOX_H = 34;
const float HEAP_GAP = 14;

// Where one cell ended up on screen.
struct Placed {
    const MemCell *cell;
    Rectangle box;   // the value box
    bool heap;
};

static bool is_pointer(const MemCell &c) { return c.value.size() > 2 && c.value.compare(0, 2, "->") == 0; }
static std::string target_of(const MemCell &c) { return c.value.substr(2); }

// An arrow made of straight segments, with a head on the last one.
static void draw_arrow(const std::vector<Vector2> &points, Color color)
{
    for (size_t i = 0; i + 1 < points.size(); i++) DrawLineEx(points[i], points[i + 1], 2, color);

    Vector2 tip = points.back(), from = points[points.size() - 2];
    float dir = (tip.x < from.x) ? 1.0f : -1.0f;   // the head points left or right
    ui_triangle(tip, { tip.x + dir * 9, tip.y - 5 }, { tip.x + dir * 9, tip.y + 5 }, color);
}

void memview_draw(const Step &step, Rectangle area)
{
    ui_panel(area, COL_SURFACE, COL_LINE, 1);

    float pad = 16;
    float heapW = 150;
    float gutter = 56;   // between the stack and the heap, where arrows run
    float stackX = area.x + pad;
    float stackW = area.width - 2 * pad - heapW - gutter;
    float heapX = stackX + stackW + gutter;
    float top = area.y + 40;

    ui_text("STACK  (newest on top)", stackX, area.y + 14, 12, COL_MUTED, FontKind::Mono);
    ui_text("HEAP", heapX, area.y + 14, 12, COL_MUTED, FontKind::Mono);

    // Group the stack cells by frame, in the order the frames first appear.
    std::vector<std::string> frames;
    std::map<std::string, std::vector<const MemCell *>> byFrame;
    std::vector<const MemCell *> heapCells;
    for (const MemCell &c : step.cells) {
        if (c.area == "heap") { heapCells.push_back(&c); continue; }
        if (byFrame.find(c.area) == byFrame.end()) frames.push_back(c.area);
        byFrame[c.area].push_back(&c);
    }

    std::vector<Placed> placed;
    std::map<std::string, size_t> byName;   // cell name -> index in placed

    // ----- Stack frames, newest (last called) first -----
    float y = top;
    for (int f = (int)frames.size() - 1; f >= 0; f--) {
        const std::vector<const MemCell *> &cells = byFrame[frames[f]];
        float h = FRAME_HEAD + cells.size() * ROW_H + 8;
        bool newest = (f == (int)frames.size() - 1) && frames.size() > 1;
        ui_panel({ stackX, y, stackW, h }, COL_BG, newest ? COL_AMBER : COL_LINE, 1);
        ui_text(frames[f] + "()", stackX + 12, y + 8, 14, newest ? COL_AMBER : COL_MUTED, FontKind::MonoBold);

        for (size_t i = 0; i < cells.size(); i++) {
            float rowY = y + FRAME_HEAD + i * ROW_H;
            Rectangle box = { stackX + stackW - 12 - VALUE_W, rowY + (ROW_H - VALUE_H) / 2, VALUE_W, VALUE_H };
            byName[cells[i]->name] = placed.size();
            placed.push_back({ cells[i], box, false });
        }
        y += h + FRAME_GAP;
    }

    // ----- Heap blocks -----
    float hy = top;
    if (heapCells.empty()) {
        Rectangle empty = { heapX, top, heapW, 120 };
        ui_panel(empty, COL_SURFACE, COL_LINE_SOFT, 1);
        float w = ui_text_width("empty", 13, FontKind::Mono);
        ui_text("empty", heapX + (heapW - w) / 2, top + 52, 13, COL_MUTED, FontKind::Mono);
    }
    for (const MemCell *c : heapCells) {
        Rectangle box = { heapX, hy + HEAP_HEAD, heapW - 24, HEAP_BOX_H };
        byName[c->name] = placed.size();
        placed.push_back({ c, box, true });
        hy += HEAP_HEAD + HEAP_BOX_H + HEAP_GAP;
    }

    // Which cells something points at (a heap block with nothing pointing at it has leaked).
    std::map<std::string, bool> pointedAt;
    for (const Placed &p : placed)
        if (is_pointer(*p.cell)) pointedAt[target_of(*p.cell)] = true;

    // ----- The cells themselves -----
    int arrowIndex = 0;
    std::map<std::string, Color> arrowColor;
    for (const Placed &p : placed) {
        const MemCell &c = *p.cell;
        Color border = COL_LINE, text = COL_TEXT, fill = COL_SURFACE;
        std::string shown = c.value;

        if (is_pointer(c)) {
            Color ac = ARROW_COLORS[arrowIndex++ % ARROW_COLOR_COUNT];
            arrowColor[c.name] = ac;
            auto t = byName.find(target_of(c));
            shown = (t != byName.end()) ? placed[t->second].cell->address : "?";
            border = ac;
            text = ac;
        }
        if (c.value == "?") text = COL_MUTED;
        bool leaked = p.heap && !pointedAt[c.name];
        if (leaked) { border = COL_CORAL; text = COL_CORAL; shown += "  leaked"; }
        // Solid colours only: ui_panel draws the border underneath the fill
        if (c.changed) { border = COL_AMBER; fill = ColorAlphaBlend(COL_SURFACE, Fade(COL_AMBER, 0.14f), WHITE); }

        ui_panel(p.box, fill, border, c.changed ? 2.0f : 1.0f);
        float w = ui_text_width(shown, 15, FontKind::Mono);
        ui_text(shown, p.box.x + (p.box.width - w) / 2, p.box.y + 8, 15, text, FontKind::Mono);

        if (p.heap) {
            std::string label = c.name + "  " + c.type + " @" + c.address;
            ui_text(label, p.box.x, p.box.y - HEAP_HEAD + 2, 12, COL_MUTED, FontKind::Mono);
        } else {
            // The name, with its type and address underneath
            float rowY = p.box.y - (ROW_H - VALUE_H) / 2;
            ui_text(c.name, stackX + 12, rowY + 3, 15, COL_TEXT, FontKind::MonoBold);
            ui_text(c.type + " @" + c.address, stackX + 12, rowY + 21, 12, COL_MUTED, FontKind::Mono);
        }
    }

    // ----- Arrows, drawn last so they sit on top -----
    int lane = 0;
    for (const Placed &p : placed) {
        if (!is_pointer(*p.cell)) continue;
        auto t = byName.find(target_of(*p.cell));
        if (t == byName.end()) continue;
        const Placed &target = placed[t->second];
        Color color = arrowColor[p.cell->name];

        Vector2 start = { p.box.x + p.box.width, p.box.y + p.box.height / 2 };
        float targetY = target.box.y + target.box.height / 2;

        if (target.heap && !p.heap) {
            // Stack to heap: across the gutter into the block's left side
            float laneX = stackX + stackW + 10 + lane * ARROW_LANE;
            draw_arrow({ start, { laneX, start.y }, { laneX, targetY }, { target.box.x - 2, targetY } }, color);
        } else if (target.heap && p.heap) {
            // Heap to heap (linked lists): down the right-hand side of the heap column
            float laneX = heapX + heapW - 12 + lane * 6;
            draw_arrow({ start, { laneX, start.y }, { laneX, targetY },
                         { target.box.x + target.box.width + 2, targetY } }, color);
        } else {
            // Stack to stack: out into the gutter and back into the target's right side
            float laneX = stackX + stackW + 10 + lane * ARROW_LANE;
            draw_arrow({ start, { laneX, start.y }, { laneX, targetY },
                         { target.box.x + target.box.width + 2, targetY } }, color);
        }
        lane++;
    }
}
