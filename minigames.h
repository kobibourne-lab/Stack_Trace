// minigames.h - the rules of the MAL-OS minigames that are worth testing on
// their own. No drawing here: screen_games.cpp draws them.
//
// Linked-list Snake: the snake is a linked list. head is the first node, each
// node's next is the one behind it, and the tail's next is nullptr. Eating a
// "new Node" adds a node; running into yourself or the wall is a segfault.

#pragma once
#include <deque>

struct Cell { int x, y; };
inline bool operator==(Cell a, Cell b) { return a.x == b.x && a.y == b.y; }

struct SnakeGame {
    int width = 24, height = 13;
    std::deque<Cell> body;      // body[0] is head
    Cell dir = { 1, 0 }, nextDir = { 1, 0 };
    Cell food = { 0, 0 };
    bool alive = true;
    int score = 0, eaten = 0;
    unsigned int seed = 12345;  // for placing food (its own, so tests are repeatable)
};

void snake_start(SnakeGame &g, unsigned int seed);
// Turn (ignored if it would reverse straight into the neck).
void snake_turn(SnakeGame &g, int dx, int dy);
// One step forward. Returns true if a node was eaten this step.
bool snake_step(SnakeGame &g);
// Seconds between steps: it speeds up as the list grows.
float snake_step_time(const SnakeGame &g);
