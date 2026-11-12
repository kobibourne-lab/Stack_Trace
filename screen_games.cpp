// screen_games.cpp - the MAL-OS minigames, played on Mal's computer screen.
//
//   snake       Linked-list Snake: the snake is a linked list (minigames.cpp has its rules)
//   heaphero    Heap Hero: free the leaked blocks before the heap fills, but
//               never a block something still points at
//   stacktower  Stack Tower: drop each recursive call's frame onto the stack,
//               then watch the calls return as it unwinds
//
// ESC goes back to the MAL-OS terminal. Best scores are saved in progress.txt.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "fx.h"
#include "sfx.h"
#include "minigames.h"
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Shared: the screen frame, game over, scores
// ---------------------------------------------------------------------------

static bool over = false;
static std::string overTitle, overLine;
static bool newBest = false;
static int finalScore = 0;
static float overAt = 0;

static void crt_frame(const std::string &status)
{
    ClearBackground({ 30, 33, 38, 255 });
    DrawRectangleRounded({ 24, 24, SCREEN_W - 48.0f, SCREEN_H - 48.0f }, 0.03f, 10, { 5, 14, 7, 255 });
    ui_text(status, 48, 36, 13, { 90, 140, 100, 255 }, FontKind::Mono);
    DrawLine(48, 60, SCREEN_W - 48, 60, { 22, 52, 28, 255 });
}

static void crt_glass(float t)
{
    for (int y = 24; y < SCREEN_H - 24; y += 3) DrawRectangle(24, y, SCREEN_W - 48, 1, Fade(BLACK, 0.16f));
    float bar = fmodf(t * 90, (float)SCREEN_H + 200) - 100;
    DrawRectangle(24, (int)bar, SCREEN_W - 48, 60, Fade(COL_GREEN, 0.02f));
    DrawRectangleRoundedLinesEx({ 24, 24, SCREEN_W - 48.0f, SCREEN_H - 48.0f }, 0.03f, 10, 6, { 30, 33, 38, 255 });
}

static std::string status_line(const App &app, const std::string &name, int score)
{
    return "MAL-OS  >  run " + name + "      score " + std::to_string(score) + "      best " +
           std::to_string(progress_best_score(app.progress, name)) + "      ESC back to MAL-OS";
}

static void game_over(App &app, const std::string &title, const std::string &line, int score)
{
    over = true;
    overTitle = title;
    overLine = line;
    finalScore = score;
    overAt = (float)GetTime();
    newBest = progress_record_score(app.progress, app.gameId, score) && score > 0;
    int crashes = ++app.progress.scores["crashes"];   // the glitch skin unlocks on the first one
    save_progress(app);
    app.mos.out.push_back({ app.gameId + " exited: score " + std::to_string(score) + (newBest ? "  NEW BEST!" : ""),
                            newBest ? MosTone::Good : MosTone::Dim });
    if (crashes == 1) {
        app.mos.out.push_back({ "Memory corrupted. Glitch skin unlocked in the wardrobe!", MosTone::Accent });
        fx_float_text("Glitch skin unlocked!", { SCREEN_W / 2.0f, 150 }, 26, COL_PURPLE);
    }
    if (newBest) { sfx_play(Sfx::LevelUp); mal_react(app.mal, MalMood::Cheer, 2.5f); }
    else { sfx_play(Sfx::Crumble); mal_react(app.mal, MalMood::Sad, 2.0f); }
    fx_shake(0.6f);
}

// Returns true to play again.
static bool draw_game_over(App &app)
{
    float age = (float)GetTime() - overAt;
    DrawRectangle(24, 24, SCREEN_W - 48, SCREEN_H - 48, Fade({ 5, 14, 7, 255 }, fminf(0.85f, age * 3)));
    float y = 200;
    float tw = ui_text_width(overTitle, 34, FontKind::MonoBold);
    ui_text(overTitle, (SCREEN_W - tw) / 2 + 3, y + 2, 34, Fade(COL_BLUE, 0.4f), FontKind::MonoBold);   // glitchy double
    ui_text(overTitle, (SCREEN_W - tw) / 2, y, 34, COL_CORAL, FontKind::MonoBold);
    float lw = ui_text_width(overLine, 16, FontKind::Mono);
    ui_text(overLine, (SCREEN_W - lw) / 2, y + 52, 16, { 90, 140, 100, 255 }, FontKind::Mono);
    std::string sc = "score " + std::to_string(finalScore) + "    best " + std::to_string(progress_best_score(app.progress, app.gameId));
    float sw = ui_text_width(sc, 22, FontKind::MonoBold);
    ui_text(sc, (SCREEN_W - sw) / 2, y + 100, 22, COL_GREEN, FontKind::MonoBold);
    if (newBest) {
        float pop = 1 + 0.15f * sinf(age * 8);
        const char *nb = "NEW BEST!";
        float nw = ui_text_width(nb, 26 * pop, FontKind::MonoBold);
        ui_text(nb, (SCREEN_W - nw) / 2, y + 140, 26 * pop, COL_AMBER, FontKind::MonoBold);
    }
    mal_draw(app.mal, { SCREEN_W / 2.0f, y + 330 }, 90);
    const char *hint = "ENTER play again      ESC back to MAL-OS";
    float hw = ui_text_width(hint, 15, FontKind::Mono);
    ui_text(hint, (SCREEN_W - hw) / 2, y + 380, 15, { 190, 225, 195, 255 }, FontKind::Mono);
    return age > 0.4f && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE));
}

// ---------------------------------------------------------------------------
// Linked-list Snake
// ---------------------------------------------------------------------------

static SnakeGame snake;
static float snakeTimer = 0;
const float CELL = 46, BOARD_X = 88, BOARD_Y = 84;

static Vector2 cell_centre(Cell c) { return { BOARD_X + c.x * CELL + CELL / 2, BOARD_Y + c.y * CELL + CELL / 2 }; }

static void snake_frame(App &app, float t)
{
    if (!over) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) snake_turn(snake, 0, -1);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) snake_turn(snake, 0, 1);
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) snake_turn(snake, -1, 0);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) snake_turn(snake, 1, 0);
        snakeTimer += GetFrameTime();
        if (snakeTimer >= snake_step_time(snake)) {
            snakeTimer = 0;
            Cell food = snake.food;
            if (snake_step(snake)) {
                Vector2 at = cell_centre(food);
                fx_burst(at, 18, COL_GREEN, COL_AMBER);
                fx_float_text("+" + std::to_string(10 + snake.eaten), { at.x, at.y - 30 }, 20, COL_AMBER);
                sfx_play(Sfx::Correct);
            }
            if (!snake.alive) {
                Cell next = { snake.body.front().x + snake.dir.x, snake.body.front().y + snake.dir.y };   // where it tried to go
                bool wall = next.x < 0 || next.y < 0 || next.x >= snake.width || next.y >= snake.height;
                game_over(app, "Segmentation fault (core dumped)",
                          std::string(wall ? "head->next went off the edge of memory" : "the list ran into its own node") +
                          ", with " + std::to_string(snake.body.size()) + " nodes", snake.score);
            }
        }
    }

    crt_frame(status_line(app, "snake", snake.score) + "      arrows / WASD steer");
    // Faint grid: memory cells
    for (int x = 0; x <= snake.width; x++) DrawLine((int)(BOARD_X + x * CELL), (int)BOARD_Y, (int)(BOARD_X + x * CELL), (int)(BOARD_Y + snake.height * CELL), { 12, 30, 16, 255 });
    for (int y = 0; y <= snake.height; y++) DrawLine((int)BOARD_X, (int)(BOARD_Y + y * CELL), (int)(BOARD_X + snake.width * CELL), (int)(BOARD_Y + y * CELL), { 12, 30, 16, 255 });

    // The food: a new node waiting to be linked in
    Vector2 f = cell_centre(snake.food);
    float pulse = 1 + 0.08f * sinf(t * 6);
    Rectangle fr = { f.x - 18 * pulse, f.y - 14 * pulse, 36 * pulse, 28 * pulse };
    DrawRectangleRounded(fr, 0.3f, 6, Fade(COL_CORAL, 0.25f));
    DrawRectangleRoundedLinesEx(fr, 0.3f, 6, 2, COL_CORAL);
    ui_text("new", f.x - ui_text_width("new", 12, FontKind::MonoBold) / 2, f.y - 8, 12, COL_CORAL, FontKind::MonoBold);

    // The list: nodes with next arrows, head first
    int n = (int)snake.body.size();
    for (int i = n - 1; i >= 0; i--) {
        Vector2 c = cell_centre(snake.body[i]);
        if (i + 1 < n) {   // the arrow to the next node
            Vector2 d = cell_centre(snake.body[i + 1]);
            DrawLineEx(c, d, 3, COL_BLUE);
        }
        bool head = i == 0;
        Rectangle r = { c.x - 18, c.y - 15, 36, 30 };
        DrawRectangleRounded(r, 0.3f, 6, head ? COL_AMBER : (i % 2 ? Color{ 90, 200, 110, 255 } : COL_GREEN));
        DrawRectangleRoundedLinesEx(r, 0.3f, 6, 2, Fade(BLACK, 0.3f));
        if (head) {
            Cell d = snake.dir;
            DrawCircleV({ c.x + d.x * 7 - d.y * 6, c.y + d.y * 7 - d.x * 6 }, 3, COL_BG);   // eyes
            DrawCircleV({ c.x + d.x * 7 + d.y * 6, c.y + d.y * 7 + d.x * 6 }, 3, COL_BG);
        } else {
            std::string v = std::to_string(i);
            ui_text(v, c.x - ui_text_width(v, 11, FontKind::MonoBold) / 2, c.y - 7, 11, COL_BG, FontKind::MonoBold);
        }
    }
    Vector2 h = cell_centre(snake.body.front());
    ui_text("head", h.x - 14, h.y - 34, 12, COL_AMBER, FontKind::MonoBold);
    Vector2 tl = cell_centre(snake.body.back());
    ui_text("-> nullptr", tl.x - 30, tl.y + 18, 11, { 90, 140, 100, 255 }, FontKind::Mono);
}

// ---------------------------------------------------------------------------
// Heap Hero
// ---------------------------------------------------------------------------

const int HEAP_COLS = 5, HEAP_ROWS = 4, SLOTS = HEAP_COLS * HEAP_ROWS, VARS = 4;
static const char *VAR_NAMES[VARS] = { "p", "q", "r", "s" };

struct HeapGame {
    bool used[SLOTS] = {};
    float leakedAt[SLOTS] = {};   // when it lost its last pointer (0 = not leaked)
    int target[VARS];             // the slot each pointer points at, -1 = nullptr
    float dangling[VARS] = {};    // > 0: shown as dangling for this long
    float timer = 0, interval = 1.6f;
    std::vector<std::string> ops;
    int score = 0, lives = 3, combo = 0, freed = 0;
};
static HeapGame heap;

static Rectangle slot_rect(int i)
{
    int c = i % HEAP_COLS, r = i / HEAP_COLS;
    return { 380 + c * 160.0f, 150 + r * 128.0f, 140, 108 };
}
static Rectangle var_rect(int v) { return { 110, 140 + v * 100.0f, 150, 60 }; }

static bool pointed_at(int slot)
{
    for (int v = 0; v < VARS; v++) if (heap.target[v] == slot) return true;
    return false;
}

static void heap_refresh_leaks()
{
    for (int i = 0; i < SLOTS; i++) {
        bool leaked = heap.used[i] && !pointed_at(i);
        if (leaked && heap.leakedAt[i] == 0) heap.leakedAt[i] = (float)GetTime();
        if (!leaked) heap.leakedAt[i] = 0;
    }
}

static void heap_start()
{
    heap = HeapGame();
    for (int v = 0; v < VARS; v++) heap.target[v] = -1;
    heap.ops.push_back("int main() {");
}

// The program runs one more line: a new, a copy or a nullptr.
static bool heap_op(App &app)
{
    int v = rand() % VARS;
    int empty = 0;
    for (int i = 0; i < SLOTS; i++) if (!heap.used[i]) empty++;
    float roll = (rand() % 100) / 100.0f;
    if (roll < 0.55f || heap.target[v] < 0) {
        if (empty == 0) {
            game_over(app, "Out of memory", "new failed: the heap is full of leaks", heap.score);
            return false;
        }
        int pick = rand() % empty;
        for (int i = 0; i < SLOTS; i++)
            if (!heap.used[i] && pick-- == 0) { heap.used[i] = true; heap.target[v] = i; break; }
        heap.ops.push_back(std::string("    ") + VAR_NAMES[v] + " = new Node;");
    } else if (roll < 0.82f) {
        int from = rand() % VARS;
        if (from == v || heap.target[from] < 0) {
            heap.target[v] = -1;
            heap.ops.push_back(std::string("    ") + VAR_NAMES[v] + " = nullptr;");
        } else {
            heap.target[v] = heap.target[from];
            heap.ops.push_back(std::string("    ") + VAR_NAMES[v] + " = " + VAR_NAMES[from] + ";");
        }
    } else {
        heap.target[v] = -1;
        heap.ops.push_back(std::string("    ") + VAR_NAMES[v] + " = nullptr;");
    }
    if (heap.ops.size() > 5) heap.ops.erase(heap.ops.begin());
    heap_refresh_leaks();
    return true;
}

static void heap_click(App &app, int i)
{
    if (!heap.used[i]) return;
    Rectangle r = slot_rect(i);
    Vector2 c = { r.x + r.width / 2, r.y + r.height / 2 };
    if (!pointed_at(i)) {   // a leak: freeing it is right
        heap.used[i] = false;
        heap.combo++;
        heap.freed++;
        int gain = 10 + 5 * (heap.combo - 1);
        heap.score += gain;
        fx_burst(c, 22, COL_GREEN, COL_AMBER);
        fx_float_text("free()  +" + std::to_string(gain), { c.x, c.y - 40 }, 18, COL_GREEN);
        sfx_play(heap.combo >= 3 ? Sfx::Combo : Sfx::Correct);
    } else {                // still in use: now those pointers dangle
        heap.used[i] = false;
        heap.combo = 0;
        heap.lives--;
        for (int v = 0; v < VARS; v++)
            if (heap.target[v] == i) { heap.target[v] = -1; heap.dangling[v] = 1.2f; }
        fx_burst(c, 18, COL_CORAL, COL_BG);
        fx_float_text("dangling pointer!", { c.x, c.y - 40 }, 18, COL_CORAL);
        fx_shake(0.5f);
        sfx_play(Sfx::Wrong);
        if (heap.lives <= 0) game_over(app, "Segmentation fault (core dumped)", "you freed memory that was still in use", heap.score);
    }
    heap_refresh_leaks();
}

static void heap_frame(App &app, float t)
{
    float dt = GetFrameTime();
    if (!over) {
        heap.timer += dt;
        if (heap.timer >= heap.interval) {
            heap.timer = 0;
            heap.interval = fmaxf(0.5f, heap.interval * 0.975f);
            heap_op(app);
        }
        for (int v = 0; v < VARS; v++) if (heap.dangling[v] > 0) heap.dangling[v] -= dt;
        Vector2 m = GetMousePosition();
        for (int i = 0; i < SLOTS && !over; i++)
            if (CheckCollisionPointRec(m, slot_rect(i)) && heap.used[i]) {
                ui_want_hand();
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) heap_click(app, i);
            }
    }

    crt_frame(status_line(app, "heaphero", heap.score) + "      click a LEAK to free it");
    // Lives, the program so far, and how full the heap is
    for (int i = 0; i < 3; i++) {
        Color c = i < heap.lives ? COL_CORAL : Color{ 40, 60, 44, 255 };
        DrawCircleV({ 1170.0f - i * 26, 86 }, 9, c);
    }
    int usedCount = 0;
    for (int i = 0; i < SLOTS; i++) if (heap.used[i]) usedCount++;
    ui_text("HEAP " + std::to_string(usedCount * 100 / SLOTS) + "% full", 380, 76, 13, COL_GREEN, FontKind::MonoBold);
    DrawRectangleRounded({ 520, 80, 400, 10 }, 1, 6, { 20, 44, 26, 255 });
    DrawRectangleRounded({ 520, 80, 400.0f * usedCount / SLOTS, 10 }, 1, 6, usedCount > SLOTS * 3 / 4 ? COL_CORAL : COL_AMBER);
    if (heap.combo >= 2) ui_text("x" + std::to_string(heap.combo) + " combo", 950, 76, 14, COL_AMBER, FontKind::MonoBold);

    ui_text("STACK", 80, 110, 13, { 90, 140, 100, 255 }, FontKind::MonoBold);
    // The code being run, bottom left, newest last
    for (size_t i = 0; i < heap.ops.size(); i++) {
        bool newest = i + 1 == heap.ops.size();
        ui_text(heap.ops[i], 60, 680 - (heap.ops.size() - i) * 19.0f, 14, newest ? COL_AMBER : Color{ 90, 140, 100, 255 }, FontKind::Mono);
    }

    // Heap blocks
    for (int i = 0; i < SLOTS; i++) {
        Rectangle r = slot_rect(i);
        if (!heap.used[i]) {
            DrawRectangleRoundedLinesEx(r, 0.12f, 6, 1, { 20, 44, 26, 255 });
            continue;
        }
        bool leak = heap.leakedAt[i] > 0;
        float pulse = leak ? 0.5f + 0.5f * sinf(t * 8) : 0;
        DrawRectangleRounded(r, 0.12f, 6, leak ? Color{ 60, 22, 22, 255 } : Color{ 14, 40, 20, 255 });
        DrawRectangleRoundedLinesEx(r, 0.12f, 6, leak ? 2 + pulse * 2 : 2, leak ? COL_CORAL : COL_GREEN);
        char addr[16];
        snprintf(addr, sizeof addr, "0x%04X", 0x9000 + i * 0x20);
        ui_text(addr, r.x + 10, r.y + 8, 12, leak ? COL_CORAL : Color{ 90, 140, 100, 255 }, FontKind::Mono);
        ui_text("Node", r.x + 10, r.y + 34, 18, leak ? COL_CORAL : COL_GREEN, FontKind::MonoBold);
        if (leak) {
            ui_text("LEAK", r.x + r.width - 50, r.y + 8, 13, COL_CORAL, FontKind::MonoBold);
            float age = fminf(1.0f, ((float)GetTime() - heap.leakedAt[i]) / 8.0f);   // how long it's been leaking
            DrawRectangle((int)r.x + 10, (int)(r.y + r.height - 16), (int)((r.width - 20) * age), 5, COL_CORAL);
        }
    }

    // Stack pointers and their arrows into the heap
    for (int v = 0; v < VARS; v++) {
        Rectangle vr = var_rect(v);
        DrawRectangleRounded(vr, 0.2f, 6, { 14, 30, 40, 255 });
        DrawRectangleRoundedLinesEx(vr, 0.2f, 6, 2, COL_BLUE);
        ui_text(VAR_NAMES[v], vr.x - 30, vr.y + 18, 22, COL_BLUE, FontKind::MonoBold);
        char held[16];
        snprintf(held, sizeof held, "0x%04X", 0x9000 + heap.target[v] * 0x20);   // the address it holds
        std::string val = heap.dangling[v] > 0 ? "dangling!" : heap.target[v] < 0 ? "nullptr" : held;
        if (heap.target[v] >= 0) ui_text(val, vr.x + 44, vr.y + 20, 15, COL_BLUE, FontKind::Mono);
        else
            ui_text(val, vr.x + 14, vr.y + 20, 15, heap.dangling[v] > 0 ? COL_CORAL : Color{ 90, 140, 100, 255 }, FontKind::Mono);
        if (heap.target[v] >= 0) {
            Rectangle sr = slot_rect(heap.target[v]);
            Vector2 a = { vr.x + vr.width - 10, vr.y + vr.height / 2 }, b = { sr.x + 4, sr.y + sr.height / 2 + (v - 1.5f) * 10 };
            DrawCircleV({ vr.x + 26, vr.y + vr.height / 2 }, 6, COL_BLUE);
            DrawSplineSegmentBezierCubic(a, { a.x + 80, a.y }, { b.x - 80, b.y }, b, 2.5f, Fade(COL_BLUE, 0.85f));
            DrawCircleV(b, 4, COL_BLUE);
        }
    }
}

// ---------------------------------------------------------------------------
// Stack Tower
// ---------------------------------------------------------------------------

struct Frame { float x, w; int n; };   // n: the call's argument, -1 for main
struct Chunk { Rectangle r; float vy; Color c; };

struct TowerGame {
    std::vector<Frame> stack;
    float x = 0, w = 0, dir = 1, speed = 300;   // the frame sliding along the top
    int callsTotal = 5, round = 1, score = 0;
    bool unwinding = false;
    float unwindTimer = 0;
    long long result = 1;
    std::vector<Chunk> chunks;
};
static TowerGame tower;
const float FRAME_H = 40, BASE_Y = 650;

static float frame_y(int index) { return BASE_Y - index * (FRAME_H + 4); }
static Color depth_color(int i) { return i == 0 ? Color{ 120, 128, 138, 255 } : (i % 2 ? COL_PURPLE : Color{ 170, 140, 255, 255 }); }

static void tower_next_frame()
{
    Frame &top = tower.stack.back();
    tower.w = top.w;
    tower.x = 60;
    tower.dir = 1;
}

static void tower_round(int round, int score)
{
    tower = TowerGame();
    tower.round = round;
    tower.score = score;
    tower.callsTotal = 3 + round * 2 > 11 ? 11 : 3 + round * 2;
    tower.speed = 260 + round * 60;
    tower.stack.push_back({ SCREEN_W / 2.0f - 220, 440, -1 });   // main()
    tower_next_frame();
}

static long long factorial(int n) { long long r = 1; for (int i = 2; i <= n; i++) r *= i; return r; }

static void tower_drop(App &app)
{
    Frame &top = tower.stack.back();
    float left = fmaxf(tower.x, top.x), right = fminf(tower.x + tower.w, top.x + top.w);
    int n = tower.callsTotal - ((int)tower.stack.size() - 1);   // this frame is fact(n)
    if (right - left <= 0) {
        tower.chunks.push_back({ { tower.x, frame_y((int)tower.stack.size()), tower.w, FRAME_H }, 0, COL_CORAL });
        game_over(app, "Segmentation fault (core dumped)", "fact(" + std::to_string(n) + ")'s frame missed the stack", tower.score);
        return;
    }
    float y = frame_y((int)tower.stack.size());
    if (fabsf(tower.x - top.x) < 6) {   // perfect: it snaps on, full width
        left = top.x;
        right = top.x + fminf(tower.w, top.w);
        tower.score += 50;
        fx_float_text("Perfect!", { left + (right - left) / 2, y - 20 }, 20, COL_AMBER);
        sfx_play(Sfx::Combo);
    } else {
        if (tower.x < left) tower.chunks.push_back({ { tower.x, y, left - tower.x, FRAME_H }, 0, COL_PURPLE });
        if (tower.x + tower.w > right) tower.chunks.push_back({ { right, y, tower.x + tower.w - right, FRAME_H }, 0, COL_PURPLE });
        sfx_play(Sfx::Click);
    }
    tower.score += (int)((right - left) / 8);
    tower.stack.push_back({ left, right - left, n });
    if (n == 0) {   // the base case: now every call returns
        tower.unwinding = true;
        tower.unwindTimer = 0.6f;
        tower.result = 1;
        fx_float_text("base case!", { left + (right - left) / 2, y - 24 }, 22, COL_GREEN);
        sfx_play(Sfx::Correct);
    } else {
        tower_next_frame();
    }
}

static void tower_frame(App &app, float t)
{
    float dt = GetFrameTime();
    if (!over) {
        if (tower.unwinding) {
            tower.unwindTimer -= dt;
            if (tower.unwindTimer <= 0) {
                Frame f = tower.stack.back();
                if (f.n < 0) {   // back to main: the round is done
                    long long value = factorial(tower.callsTotal);
                    tower.score += tower.callsTotal * 100;
                    fx_float_text("fact(" + std::to_string(tower.callsTotal) + ") = " + std::to_string(value) + "   round " +
                                  std::to_string(tower.round) + " done!", { SCREEN_W / 2.0f, 300 }, 26, COL_GREEN);
                    sfx_play(Sfx::LevelUp);
                    mal_react(app.mal, MalMood::Cheer, 2);
                    tower_round(tower.round + 1, tower.score);
                } else {
                    long long value = factorial(f.n);
                    Vector2 c = { f.x + f.w / 2, frame_y((int)tower.stack.size() - 1) };
                    fx_burst(c, 14, COL_PURPLE, COL_AMBER);
                    fx_float_text("fact(" + std::to_string(f.n) + ") returns " + std::to_string(value), { c.x, c.y - 10 }, 18, COL_GREEN);
                    sfx_play(Sfx::Click);
                    tower.stack.pop_back();
                    tower.unwindTimer = 0.42f;
                }
            }
        } else {
            tower.x += tower.dir * tower.speed * dt;
            if (tower.x < 60) { tower.x = 60; tower.dir = 1; }
            if (tower.x + tower.w > SCREEN_W - 60) { tower.x = SCREEN_W - 60 - tower.w; tower.dir = -1; }
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) tower_drop(app);
        }
    }
    for (Chunk &c : tower.chunks) { c.vy += 900 * dt; c.r.y += c.vy * dt; }
    for (size_t i = 0; i < tower.chunks.size();) { if (tower.chunks[i].r.y > SCREEN_H) tower.chunks.erase(tower.chunks.begin() + i); else i++; }

    crt_frame(status_line(app, "stacktower", tower.score) + "      SPACE drops the frame");
    ui_text("round " + std::to_string(tower.round) + ":  fact(" + std::to_string(tower.callsTotal) + ")", 60, 76, 16, COL_AMBER, FontKind::MonoBold);
    ui_text("int fact(int n) { if (n == 0) return 1; return n * fact(n - 1); }", 60, 100, 13, { 90, 140, 100, 255 }, FontKind::Mono);

    // The stack, main at the bottom
    for (int i = 0; i < (int)tower.stack.size(); i++) {
        const Frame &f = tower.stack[i];
        Rectangle r = { f.x, frame_y(i), f.w, FRAME_H };
        DrawRectangleRounded(r, 0.2f, 6, depth_color(i));
        DrawRectangleRoundedLinesEx(r, 0.2f, 6, 2, Fade(BLACK, 0.35f));
        std::string label = f.n < 0 ? "main()" : "fact(" + std::to_string(f.n) + ")   n = " + std::to_string(f.n);
        float lw = ui_text_width(label, 14, FontKind::MonoBold);
        if (lw + 12 < f.w) ui_text(label, r.x + (r.width - lw) / 2, r.y + 11, 14, COL_BG, FontKind::MonoBold);
        else if (f.n >= 0) ui_text(std::to_string(f.n), r.x + r.width / 2 - 4, r.y + 11, 14, COL_BG, FontKind::MonoBold);
    }
    // The frame sliding along the top
    if (!tower.unwinding && !over) {
        int n = tower.callsTotal - ((int)tower.stack.size() - 1);
        Rectangle r = { tower.x, frame_y((int)tower.stack.size()), tower.w, FRAME_H };
        DrawRectangleRounded(r, 0.2f, 6, Fade(COL_AMBER, 0.9f));
        std::string label = "fact(" + std::to_string(n) + ")";
        float lw = ui_text_width(label, 14, FontKind::MonoBold);
        if (lw + 12 < tower.w) ui_text(label, r.x + (r.width - lw) / 2, r.y + 11, 14, COL_BG, FontKind::MonoBold);
        ui_text("call", r.x - 44, r.y + 12, 13, COL_AMBER, FontKind::Mono);
    }
    if (tower.unwinding) ui_text("returning...", SCREEN_W - 230, 76, 16, COL_GREEN, FontKind::MonoBold);
    for (const Chunk &c : tower.chunks) DrawRectangleRounded(c.r, 0.2f, 6, Fade(c.c, 0.7f));
    (void)t;
}

// ---------------------------------------------------------------------------

void start_game(App &app, const std::string &id)
{
    app.gameId = id;
    app.screen = Screen::Game;
    over = false;
    newBest = false;
    srand((unsigned int)(GetTime() * 1000));
    if (id == "snake") { snake_start(snake, (unsigned int)rand()); snakeTimer = 0; }
    if (id == "heaphero") heap_start();
    if (id == "stacktower") tower_round(1, 0);
}

void screen_games(App &app)
{
    float t = (float)GetTime();
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (!over) {
            int score = app.gameId == "snake" ? snake.score : app.gameId == "heaphero" ? heap.score : tower.score;
            if (progress_record_score(app.progress, app.gameId, score)) save_progress(app);
            app.mos.out.push_back({ app.gameId + " exited: score " + std::to_string(score), MosTone::Dim });
        }
        app.screen = Screen::Room;   // back at the terminal, where we left it
        return;
    }
    if (app.gameId == "snake") snake_frame(app, t);
    else if (app.gameId == "heaphero") heap_frame(app, t);
    else if (app.gameId == "stacktower") tower_frame(app, t);
    else { app.screen = Screen::Room; return; }

    crt_glass(t);
    if (over && draw_game_over(app)) start_game(app, app.gameId);
}
