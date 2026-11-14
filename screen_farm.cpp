// screen_farm.cpp - the Memory Farm, through the door in Mal's room. A field
// of plots seen from an angle, one crop per topic, with the code tree in the
// middle. Crops grow with mastery and wilt when a topic needs reviewing:
// click one to quiz it (that waters it), or harvest it for bytes when it's in
// fruit. The drone flies the plots in a for loop and harvests for you, with
// its code (drone.c) running in the corner. The rules are in farm.cpp.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include <cmath>
#include <vector>

const int GRID = 5;                         // 5 x 5 plots, the middle one is the tree
const float TILE_W = 132, TILE_H = 66;      // a plot's diamond
const Vector2 FARM_CENTRE = { 560, 410 };   // the middle of the field (the tree)

static bool treeOpen = false;
static float droneT = 0;          // where the drone is: plot index + how far to the next
static int droneLine = 0;         // the line of drone.c running now
static int droneDone = 0;         // plots visited this lap

void open_farm(App &app)
{
    if (app.screen != Screen::Farm) app.returnTo = app.screen;
    app.screen = Screen::Farm;
    treeOpen = false;
}

// Plot k's grid cell (skipping the middle), and where its centre is on screen.
static void plot_cell(int k, int &gx, int &gy)
{
    int cell = k < (GRID * GRID) / 2 ? k : k + 1;
    gx = cell % GRID;
    gy = cell / GRID;
}

static Vector2 cell_centre(int gx, int gy)
{
    float half = (GRID - 1) / 2.0f;
    return { FARM_CENTRE.x + (gx - gy) * TILE_W / 2, FARM_CENTRE.y + (gx + gy - 2 * half) * TILE_H / 2 };
}

static void diamond(Vector2 c, float w, float h, Color col)
{
    Vector2 top = { c.x, c.y - h / 2 }, right = { c.x + w / 2, c.y }, bottom = { c.x, c.y + h / 2 }, left = { c.x - w / 2, c.y };
    ui_triangle(top, right, bottom, col);
    ui_triangle(top, bottom, left, col);
}

static Color track_colour(const std::string &track)
{
    if (track.find("basics") != std::string::npos) return COL_BLUE;
    if (track.find("Pointer") != std::string::npos) return COL_PURPLE;
    if (track.find("Linked") != std::string::npos) return COL_CORAL;
    return COL_AMBER;
}

// A crop: a seed, a sprout, a plant, a tall plant, or one in fruit. Wilting droops it brown.
static void draw_crop(Vector2 base, int stage, bool wilting, Color fruit, float t, float seed)
{
    float sway = sinf(t * 1.6f + seed) * 2;
    Color stem = wilting ? Color{ 150, 120, 60, 255 } : Color{ 70, 160, 80, 255 };
    Color leaf = wilting ? Color{ 170, 140, 70, 255 } : Color{ 100, 200, 110, 255 };
    float droop = wilting ? 0.55f : 1;
    if (stage == 0) {   // a seed in a little mound of soil
        DrawEllipse((int)base.x, (int)base.y, 12, 6, { 92, 60, 38, 255 });
        DrawEllipse((int)base.x, (int)base.y - 2, 9, 4, { 140, 96, 62, 255 });
        DrawCircleV({ base.x + 2, base.y - 4 }, 3, { 220, 190, 120, 255 });
        return;
    }
    float h = (stage == 1 ? 22 : stage == 2 ? 42 : 62) * (wilting ? 0.8f : 1);
    Vector2 top = { base.x + sway * (stage / 2.0f) + (wilting ? 8 : 0), base.y - h };
    DrawLineEx(base, top, stage >= 3 ? 4 : 3, stem);
    int leaves = stage == 1 ? 1 : stage == 2 ? 2 : 3;
    for (int i = 0; i < leaves; i++) {
        float u = 0.35f + 0.25f * i;
        Vector2 at = { base.x + (top.x - base.x) * u, base.y + (top.y - base.y) * u };
        for (int side = -1; side <= 1; side += 2) {
            Vector2 tip = { at.x + side * 16, at.y - 9 * droop + (wilting ? 11 : 0) };
            DrawLineEx(at, tip, 7, leaf);
        }
    }
    if (stage == 4 && !wilting) {   // in fruit: three round fruits in the topic's colour
        for (int i = 0; i < 3; i++) {
            Vector2 f = { top.x + (i - 1) * 11.0f, top.y + 5 + (i == 1 ? -8 : 0) };
            DrawCircleV(f, 8, fruit);
            DrawCircleV({ f.x - 3, f.y - 3 }, 2.5f, Fade(WHITE, 0.6f));
        }
    } else if (stage == 3) {
        DrawCircleV(top, 6, wilting ? stem : Fade(fruit, 0.6f));   // a bud
    }
}

// The bytes a plot shows when it's ready to harvest.
static void draw_coin(Vector2 at, int bytes, float t)
{
    float hop = fabsf(sinf(t * 4)) * 5;
    DrawCircleV({ at.x, at.y - hop }, 11, COL_PURPLE);
    DrawCircleV({ at.x, at.y - hop }, 8, { 180, 140, 230, 255 });
    std::string n = std::to_string(bytes);
    ui_text(n, at.x - ui_text_width(n, 10, FontKind::MonoBold) / 2, at.y - hop - 6, 10, COL_BG, FontKind::MonoBold);
}

static void draw_drone(Vector2 at, float t)
{
    float bob = sinf(t * 5) * 3;
    Vector2 c = { at.x, at.y + bob };
    DrawEllipse((int)at.x, (int)(at.y + 80), 22, 6, Fade(BLACK, 0.18f));   // its shadow on the ground
    DrawLineEx({ c.x - 34, c.y - 10 }, { c.x + 34, c.y - 10 }, 3, { 60, 66, 74, 255 });
    for (int side = -1; side <= 1; side += 2) {   // spinning rotors
        float spin = fabsf(sinf(t * 40 + side));
        DrawEllipse((int)(c.x + side * 34), (int)(c.y - 14), 22 * spin + 4, 3, Fade(COL_TEXT_SOFT, 0.8f));
    }
    DrawRectangleRounded({ c.x - 24, c.y - 8, 48, 18 }, 0.5f, 6, { 40, 44, 52, 255 });
    DrawCircleV({ c.x - 8, c.y + 1 }, 4, COL_GREEN);
    DrawCircleV({ c.x + 8, c.y + 1 }, 4, COL_GREEN);
}

static const char *DRONE_C[] = {
    "for (int i = 0; i < 20; i++) {",
    "    drone_fly(i);",
    "    if (wilting(i))",
    "        flag(i);    // you review it",
    "    if (ripe(i))",
    "        harvest(i);",
    "}",
};

void screen_farm(App &app)
{
    float t = (float)GetTime(), dt = GetFrameTime();
    std::string today = today_string();
    int count = (int)app.topics.size() < 24 ? (int)app.topics.size() : 24;
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (treeOpen) treeOpen = false;
        else { open_room(app); return; }
    }

    // ----- Sky, sun, clouds and the field -----
    DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, { 255, 196, 140, 255 }, { 255, 226, 180, 255 });
    DrawCircleV({ 1040, 120 }, 64, { 255, 240, 190, 255 });
    DrawCircleV({ 1040, 120 }, 84, Fade({ 255, 240, 190, 255 }, 0.3f));
    for (int i = 0; i < 3; i++) {
        float cx = fmodf(160 + i * 380 + t * (8 + i * 3), SCREEN_W + 200) - 100;
        DrawEllipse((int)cx, 90 + i * 30, 70, 18, Fade(WHITE, 0.85f));
        DrawEllipse((int)cx + 40, 82 + i * 30, 50, 16, Fade(WHITE, 0.85f));
    }
    float half = (GRID - 1) / 2.0f + 0.9f;   // the field is a big diamond a bit wider than the plots
    Vector2 top = { FARM_CENTRE.x, FARM_CENTRE.y - half * TILE_H }, right = { FARM_CENTRE.x + half * TILE_W, FARM_CENTRE.y },
            bottom = { FARM_CENTRE.x, FARM_CENTRE.y + half * TILE_H }, left = { FARM_CENTRE.x - half * TILE_W, FARM_CENTRE.y };
    float depth = 28;   // the side of the slab of earth
    ui_triangle(left, bottom, { bottom.x, bottom.y + depth }, { 64, 140, 60, 255 });
    ui_triangle(left, { bottom.x, bottom.y + depth }, { left.x, left.y + depth }, { 64, 140, 60, 255 });
    ui_triangle(bottom, right, { right.x, right.y + depth }, { 52, 120, 52, 255 });
    ui_triangle(bottom, { right.x, right.y + depth }, { bottom.x, bottom.y + depth }, { 52, 120, 52, 255 });
    ui_triangle(top, right, bottom, { 110, 196, 96, 255 });
    ui_triangle(top, bottom, left, { 110, 196, 96, 255 });

    // ----- The barn: back to Mal's room -----
    Rectangle barn = { 940, 250, 150, 130 };
    bool overBarn = !treeOpen && CheckCollisionPointRec(GetMousePosition(), { barn.x, barn.y - 50, barn.width, barn.height + 50 });
    DrawTriangle({ barn.x - 10, barn.y }, { barn.x + barn.width + 10, barn.y }, { barn.x + barn.width / 2, barn.y - 50 }, { 240, 230, 220, 255 });
    DrawTriangle({ barn.x + 2, barn.y }, { barn.x + barn.width - 2, barn.y }, { barn.x + barn.width / 2, barn.y - 42 }, { 190, 50, 46, 255 });
    DrawRectangleRec(barn, { 200, 56, 50, 255 });
    DrawRectangle((int)(barn.x + 55), (int)(barn.y + 60), 40, 70, { 120, 34, 32, 255 });
    ui_text("MAL'S ROOM", barn.x + 30, barn.y + 18, 13, { 255, 236, 220, 255 }, FontKind::MonoBold);
    if (overBarn) {
        DrawRectangleRoundedLinesEx({ barn.x - 16, barn.y - 56, barn.width + 32, barn.height + 62 }, 0.1f, 8, 3, Fade(COL_AMBER, 0.6f + 0.3f * sinf(t * 5)));
        ui_want_hand();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { sfx_play(Sfx::Click); open_room(app); return; }
    }

    // ----- The plots and the tree, back to front -----
    int hoverPlot = -1;
    bool hoverTree = false;
    Vector2 mouse = GetMousePosition();
    int ripe = 0, wilting = 0;
    struct Tag { Vector2 at; int value; bool wilt; int k; };
    std::vector<Tag> tags;   // drawn after everything, so the tree doesn't hide them
    for (int d = 0; d <= 2 * (GRID - 1); d++) {   // a diagonal at a time, so nearer things draw over farther ones
        for (int gx = 0; gx < GRID; gx++) {
            int gy = d - gx;
            if (gy < 0 || gy >= GRID) continue;
            Vector2 c = cell_centre(gx, gy);
            if (gx == GRID / 2 && gy == GRID / 2) {   // the middle: the code tree
                Rectangle area = { c.x - 125, c.y - 262, 250, 280 };
                hoverTree = !treeOpen && CheckCollisionPointRec(mouse, { area.x + 30, area.y + 30, area.width - 60, area.height - 30 });
                diamond(c, TILE_W - 10, TILE_H - 6, { 86, 170, 80, 255 });
                draw_code_tree(app, area, t, false);
                if (hoverTree) DrawEllipseLines((int)c.x, (int)c.y, 70, 26, Fade(COL_AMBER, 0.8f));
                continue;
            }
            int k = gy * GRID + gx;
            k = k > (GRID * GRID) / 2 ? k - 1 : k;
            bool used = k < count;
            bool over = !treeOpen && (CheckCollisionPointTriangle(mouse, { c.x, c.y - TILE_H / 2 }, { c.x + TILE_W / 2, c.y }, { c.x, c.y + TILE_H / 2 }) ||
                                      CheckCollisionPointTriangle(mouse, { c.x, c.y - TILE_H / 2 }, { c.x, c.y + TILE_H / 2 }, { c.x - TILE_W / 2, c.y }));
            if (!used) {   // a spare corner: a flower bed
                diamond(c, TILE_W - 14, TILE_H - 8, { 96, 182, 88, 255 });
                for (int f = 0; f < 5; f++)
                    DrawCircleV({ c.x - 30 + f * 15.0f, c.y - 4 + (f % 2) * 8.0f }, 4, f % 2 ? COL_CORAL : Color{ 255, 220, 120, 255 });
                continue;
            }
            const Topic &tp = app.topics[k];
            int stage = crop_stage(app.progress, tp.id);
            bool wilt = crop_wilting(app.progress, tp.id, today);
            int value = crop_harvest_value(app.progress, tp.id, today);
            if (value > 0) ripe++;
            if (wilt) wilting++;
            if (over) hoverPlot = k;
            // The soil, with furrows; dry and pale when wilting
            Color soil = wilt ? Color{ 160, 120, 80, 255 } : Color{ 120, 80, 50, 255 };
            diamond({ c.x, c.y + 4 }, TILE_W - 12, TILE_H - 6, { 80, 54, 34, 255 });
            diamond(c, TILE_W - 12, TILE_H - 6, over ? Color{ (unsigned char)(soil.r + 20), (unsigned char)(soil.g + 16), (unsigned char)(soil.b + 12), 255 } : soil);
            for (int f = -1; f <= 1; f++)
                DrawLineEx({ c.x - 36 + f * 14.0f, c.y - 10 + f * 8.0f }, { c.x + 14 + f * 14.0f, c.y + 16 + f * 8.0f }, 1.5f, Fade(BLACK, 0.18f));
            draw_crop({ c.x, c.y + 6 }, stage, wilt, track_colour(tp.track), t, (float)k);
            if (wilt || value > 0) tags.push_back({ c, value, wilt, k });
        }
    }
    for (const Tag &g : tags) {
        if (g.wilt) {   // a little tag: review me
            const char *tag = "review!";
            float tw = ui_text_width(tag, 11, FontKind::MonoBold) + 12;
            ui_panel({ g.at.x - tw / 2, g.at.y - 76, tw, 18 }, { 40, 30, 30, 230 }, COL_CORAL, 1);
            ui_text(tag, g.at.x - tw / 2 + 6, g.at.y - 73, 11, COL_CORAL, FontKind::MonoBold);
        } else {
            draw_coin({ g.at.x + 30, g.at.y - 56 }, g.value, t + g.k);
        }
    }

    // ----- The drone: flies plot to plot in a for loop -----
    bool drone = farm_has_drone(app.progress);
    if (drone && count > 0 && !treeOpen) {
        float before = droneT;
        droneT += dt / 1.1f;
        if (droneT >= count) { droneT = 0; droneDone = 0; }
        int i = (int)droneT;
        float u = droneT - i;
        if ((int)before != i || before == 0) {   // arrived at plot i: does it need anything?
            droneDone = i + 1;
            const Topic &tp = app.topics[i];
            int got = crop_harvest(app.progress, tp.id, today);
            int gx, gy;
            plot_cell(i, gx, gy);
            Vector2 c = cell_centre(gx, gy);
            if (got > 0) {
                save_progress(app);
                fx_float_text("+" + std::to_string(got) + " bytes", { c.x, c.y - 70 }, 18, COL_PURPLE);
                fx_burst({ c.x, c.y - 30 }, 14, COL_PURPLE, COL_GREEN);
                quest_event(app, "harvest");
                droneLine = 6;
            } else if (crop_wilting(app.progress, tp.id, today)) {
                droneLine = 4;
            } else {
                droneLine = 2;
            }
        }
        int gx, gy, nx, ny;
        plot_cell(i, gx, gy);
        plot_cell((i + 1) % count, nx, ny);
        Vector2 a = cell_centre(gx, gy), b = cell_centre(nx, ny);
        float e = u * u * (3 - 2 * u);
        draw_drone({ a.x + (b.x - a.x) * e, a.y + (b.y - a.y) * e - 110 }, t);
        if (u > 0.5f) droneLine = 2;
    }

    // ----- Mal, in the field -----
    mal_set_base(app.mal, wilting > 0 ? MalMood::Idle : MalMood::Happy);
    mal_draw_with_pet(app.mal, { 190, 560 }, 96);

    // ----- The header: bytes, what's ripe, what's wilting -----
    ui_panel({ 20, 16, 560, 58 }, Fade({ 40, 34, 30, 255 }, 0.92f), Fade(BLACK, 0), 0);
    ui_text("MEMORY FARM", 40, 32, 22, { 240, 236, 228, 255 }, FontKind::MonoBold);
    std::string info = std::to_string(app.progress.bytes) + " bytes  |  " + std::to_string(ripe) + " ready  |  " + std::to_string(wilting) + " wilting";
    ui_text(info, 230, 37, 14, COL_AMBER, FontKind::MonoBold);
    if (ui_button("Back to the room", { SCREEN_W - 40 - 190, 18, 190, 44 }, ButtonStyle::Primary, COL_AMBER)) { open_room(app); return; }

    // ----- drone.c, or buying the drone -----
    Rectangle panel = { 930, 432, 320, 222 };
    ui_panel(panel, { 22, 26, 32, 240 }, COL_LINE, 1);
    ui_text("drone.c", panel.x + 18, panel.y + 14, 14, COL_AMBER, FontKind::MonoBold);
    for (int i = 0; i < 7; i++) {
        bool on = drone && i + 1 == droneLine;
        if (on) DrawRectangle((int)panel.x + 8, (int)(panel.y + 38 + i * 21), (int)panel.width - 16, 20, Fade(COL_AMBER, 0.15f));
        ui_text(DRONE_C[i], panel.x + 18, panel.y + 41 + i * 21, 12, on ? COL_TEXT : (drone ? COL_TEXT_SOFT : COL_MUTED), FontKind::Mono);
    }
    if (drone) {
        ui_text("> running  |  plot " + std::to_string(droneDone) + " of " + std::to_string(count), panel.x + 18, panel.y + panel.height - 26, 12,
                COL_GREEN, FontKind::Mono);
    } else {
        bool afford = app.progress.bytes >= DRONE_PRICE;
        if (ui_button("Buy the drone  " + std::to_string(DRONE_PRICE) + " bytes", { panel.x + 18, panel.y + panel.height - 50, panel.width - 36, 38 },
                      afford ? ButtonStyle::Primary : ButtonStyle::Disabled, COL_GREEN)) {
            std::string why;
            if (farm_buy_drone(app.progress, why)) {
                save_progress(app);
                sfx_play(Sfx::LevelUp);
                fx_burst({ panel.x + panel.width / 2, panel.y }, 50, COL_GREEN, COL_AMBER);
                droneT = 0;
            }
        }
    }

    // ----- What's under the mouse, and clicking it -----
    if (hoverPlot >= 0) {
        const Topic &tp = app.topics[hoverPlot];
        int stage = crop_stage(app.progress, tp.id);
        bool wilt = crop_wilting(app.progress, tp.id, today);
        int value = crop_harvest_value(app.progress, tp.id, today);
        int left = crop_days_left(app.progress, tp.id, today);
        static const char *stages[] = { "a seed", "a sprout", "growing", "nearly there", "in fruit" };
        std::string l1 = tp.title;
        std::string l2 = std::string(stages[stage]) + "  |  " + mastery_name(topic_mastery(app.progress, tp.id));
        std::string l3 = value > 0 ? "Click to harvest: +" + std::to_string(value) + " bytes"
                       : wilt ? "Wilting! Click to quiz it and water it."
                       : stage == 0 ? "Click to plant it: start the lesson."
                       : "Fresh for " + std::to_string(left) + (left == 1 ? " more day" : " more days") + ". Click to quiz it.";
        float w = fmaxf(ui_text_width(l1, 15, FontKind::UiBold), fmaxf(ui_text_width(l2, 12, FontKind::Mono), ui_text_width(l3, 13))) + 24;
        float x = fminf(mouse.x + 18, SCREEN_W - w - 10), y = mouse.y + 16;
        ui_panel({ x, y, w, 74 }, Fade(COL_BG, 0.94f), wilt ? COL_CORAL : value > 0 ? COL_PURPLE : COL_GREEN, 1);
        ui_text(l1, x + 12, y + 8, 15, COL_TEXT, FontKind::UiBold);
        ui_text(l2, x + 12, y + 30, 12, COL_MUTED, FontKind::Mono);
        ui_text(l3, x + 12, y + 50, 13, wilt ? COL_CORAL : value > 0 ? COL_PURPLE : COL_TEXT_SOFT);
        ui_want_hand();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int gx, gy;
            plot_cell(hoverPlot, gx, gy);
            Vector2 c = cell_centre(gx, gy);
            if (value > 0) {
                int got = crop_harvest(app.progress, tp.id, today);
                save_progress(app);
                sfx_play(Sfx::Combo);
                fx_burst({ c.x, c.y - 30 }, 24, COL_PURPLE, COL_GREEN);
                fx_float_text("+" + std::to_string(got) + " bytes", { c.x, c.y - 70 }, 20, COL_PURPLE);
                quest_event(app, "harvest");
            } else {
                app.selected = hoverPlot;   // the topic map's selection: the lesson and quiz open for it
                app.inCampaign = false;
                if (stage == 0) open_lesson(app);
                else open_quiz(app);
                return;
            }
        }
    } else if (hoverTree) {
        ui_want_hand();
        const char *tip = "Your code tree: click to look closer";
        float w = ui_text_width(tip, 14, FontKind::UiBold) + 20;
        ui_panel({ mouse.x + 16, mouse.y + 14, w, 28 }, Fade(COL_BG, 0.9f), COL_AMBER, 1);
        ui_text(tip, mouse.x + 26, mouse.y + 20, 14, COL_TEXT, FontKind::UiBold);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { treeOpen = true; sfx_play(Sfx::Click); }
    }

    DrawRectangle(0, SCREEN_H - 40, SCREEN_W, 40, Fade({ 30, 26, 22, 255 }, 0.9f));
    ui_text("every topic is a crop  |  review before it wilts  |  harvest ripe ones for bytes  |  ESC back to the room", 40, SCREEN_H - 27, 13,
            { 220, 210, 196, 255 }, FontKind::Mono);

    if (treeOpen) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.6f));
        Rectangle p = { 140, 50, SCREEN_W - 280.0f, SCREEN_H - 100.0f };
        ui_panel(p, { 12, 22, 40, 255 }, COL_GREEN, 2);
        ui_text("YOUR CODE TREE", p.x + 28, p.y + 22, 13, COL_GREEN, FontKind::MonoBold);
        ui_text("A branch for each region, a leaf for each topic. Leaves grow as you learn; gold blossoms are Mastered,"
                " and stars mark beaten bosses.", p.x + 28, p.y + 42, 14, COL_TEXT_SOFT);
        draw_code_tree(app, { p.x + 40, p.y + 80, p.width - 80, p.height - 130 }, t, true);
        if (ui_button("Close", { p.x + p.width - 28 - 120, p.y + p.height - 28 - 44, 120, 44 }, ButtonStyle::Outline, COL_TEXT))
            treeOpen = false;
    }
}
