// minigames.cpp - Linked-list Snake's rules.

#include "minigames.h"

static unsigned int next_rand(unsigned int &seed)
{
    seed = seed * 1103515245u + 12345u;
    return (seed >> 16) & 0x7fff;
}

static bool on_snake(const SnakeGame &g, Cell c)
{
    for (const Cell &b : g.body) if (b == c) return true;
    return false;
}

static void place_food(SnakeGame &g)
{
    for (int tries = 0; tries < 1000; tries++) {
        Cell c = { (int)(next_rand(g.seed) % g.width), (int)(next_rand(g.seed) % g.height) };
        if (!on_snake(g, c)) { g.food = c; return; }
    }
}

void snake_start(SnakeGame &g, unsigned int seed)
{
    int w = g.width, h = g.height;
    g = SnakeGame();
    g.width = w; g.height = h;
    g.seed = seed;
    int y = g.height / 2;
    for (int i = 0; i < 3; i++) g.body.push_back({ 5 - i, y });   // head, then two nodes behind
    place_food(g);
}

void snake_turn(SnakeGame &g, int dx, int dy)
{
    if (dx == -g.dir.x && dy == -g.dir.y) return;   // no reversing into your own neck
    g.nextDir = { dx, dy };
}

bool snake_step(SnakeGame &g)
{
    if (!g.alive) return false;
    g.dir = g.nextDir;
    Cell head = { g.body.front().x + g.dir.x, g.body.front().y + g.dir.y };
    bool eats = head == g.food;

    // The tail moves out of the way this step unless the snake is growing
    bool hitsSelf = false;
    for (size_t i = 0; i < g.body.size(); i++) {
        if (!eats && i == g.body.size() - 1) break;
        if (g.body[i] == head) hitsSelf = true;
    }
    if (head.x < 0 || head.y < 0 || head.x >= g.width || head.y >= g.height || hitsSelf) {
        g.alive = false;   // segfault
        return false;
    }
    g.body.push_front(head);
    if (eats) {
        g.eaten++;
        g.score += 10 + g.eaten;   // later nodes are worth a bit more
        place_food(g);
    } else {
        g.body.pop_back();
    }
    return eats;
}

float snake_step_time(const SnakeGame &g)
{
    float t = 0.16f - g.eaten * 0.004f;
    return t < 0.065f ? 0.065f : t;
}
