// screen_campaign.cpp - the campaign's world map: one column per region of
// memory (The Stack, Address Space, The Heap, Object Land), a path that snakes
// through every topic, and a boss at the end of each region. Mal stands on the
// next thing to play. The rules (what's cleared, what's open) are in campaign.cpp.
//
// Each region is drawn as its own themed world (worlds.cpp): hills, a cave, a
// sea of islands, a castle city. Topics are raised platforms, and Mal hops
// from stop to stop. Clearing a topic or opening a boss gets a burst.
//
// Zoom out (scroll down, M, or the 3D city button) and the camera pulls back
// to Memory City (memcity.cpp): the same campaign as floating 3D islands.
// Click an island to dive back into its world. Beating a boss zooms out on
// its own, lights the bridge to the next island, and dives into it.

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "sfx.h"
#include "fx.h"
#include "worlds.h"
#include "memcity.h"
#include <cmath>

const float MAP_TOP = HEADER_H;
const float NODE_TOP = MAP_TOP + 160;      // the highest node
const float NODE_BOTTOM = MAP_TOP + 440;   // the lowest node
const float ZIG = 40;                      // how far topics sit left and right of the column's centre
const float LABEL_W = 92;

enum class NodeState { Done, Current, Todo, Locked };

struct MapNode {
    int region;
    int topic;          // index into the region's topics, or -1 for its boss
    Vector2 pos;
    NodeState state;
};

static int topic_index(const App &app, const std::string &id)
{
    for (int i = 0; i < (int)app.topics.size(); i++)
        if (app.topics[i].id == id) return i;
    return -1;
}

static std::string topic_title(const App &app, const std::string &id)
{
    int i = topic_index(app, id);
    return i < 0 ? id : app.topics[i].title;
}

static float column_width(const App &app) { return (SCREEN_W - 48.0f) / (float)app.campaign.size(); }
static float column_centre(const App &app, int region) { return 24 + column_width(app) * (region + 0.5f); }

// Every node, in the order the path visits them. Even columns go up the
// screen, odd ones come back down, so the path snakes from region to region.
static std::vector<MapNode> build_nodes(const App &app)
{
    std::vector<MapNode> nodes;
    const Progress &p = app.progress;
    int playing = current_region(app.campaign, p);
    for (int r = 0; r < (int)app.campaign.size(); r++) {
        const Region &reg = app.campaign[r];
        bool open = region_open(app.campaign, p, r);
        int next = next_topic(reg, p);
        int count = (int)reg.topics.size() + 1;
        float cx = column_centre(app, r);
        for (int i = 0; i < count; i++) {
            float t = count > 1 ? (float)i / (count - 1) : 0;
            float y = (r % 2 == 0) ? NODE_BOTTOM - t * (NODE_BOTTOM - NODE_TOP) : NODE_TOP + t * (NODE_BOTTOM - NODE_TOP);
            bool boss = i == count - 1;
            MapNode n;
            n.region = r;
            n.topic = boss ? -1 : i;
            n.pos = { boss ? cx : cx + (i % 2 == 0 ? ZIG : -ZIG), y };
            if (!open) n.state = NodeState::Locked;
            else if (boss) n.state = region_beaten(app.campaign, p, r) ? NodeState::Done
                                   : boss_unlocked(app.campaign, p, r) ? NodeState::Current : NodeState::Todo;
            else if (topic_cleared(p, reg.topics[i])) n.state = NodeState::Done;
            else n.state = (r == playing && i == next) ? NodeState::Current : NodeState::Todo;
            nodes.push_back(n);
        }
    }
    return nodes;
}

static bool is_selected(const App &app, const MapNode &n)
{
    return n.region == app.campaignRegion && n.topic == app.campaignTopic;
}

// ---------------------------------------------------------------------------
// Drawing pieces
// ---------------------------------------------------------------------------

// Words split into lines no wider than width.
static std::vector<std::string> wrap(const std::string &text, float width, float size, FontKind font)
{
    std::vector<std::string> lines;
    std::string line, word;
    for (size_t i = 0; i <= text.size(); i++) {
        if (i == text.size() || text[i] == ' ') {
            std::string tryLine = line.empty() ? word : line + " " + word;
            if (!line.empty() && ui_text_width(tryLine, size, font) > width) {
                lines.push_back(line);
                line = word;
            } else {
                line = tryLine;
            }
            word.clear();
        } else {
            word += text[i];
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// A dotted line, for the part of the path still to come.
static void dotted_line(Vector2 a, Vector2 b, Color color)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx * dx + dy * dy);
    for (float d = 0; d <= len; d += 12) DrawCircleV({ a.x + dx * d / len, a.y + dy * d / len }, 2, color);
}

static void tick(Vector2 c, float s, Color color)
{
    DrawLineEx({ c.x - s, c.y }, { c.x - s * 0.3f, c.y + s * 0.7f }, 3, color);
    DrawLineEx({ c.x - s * 0.3f, c.y + s * 0.7f }, { c.x + s, c.y - s * 0.6f }, 3, color);
}

// A topic: a raised platform in its world's colours. Green with a tick once
// cleared, amber and pulsing for the one to play next, grey while locked.
static void draw_topic_node(const MapNode &n, bool selected, bool hover)
{
    Vector2 c = n.pos;
    float t = (float)GetTime();
    float rx = n.state == NodeState::Current ? 27 : 23;
    if (selected || hover) {
        float lift = selected ? 2 + 1.5f * sinf(t * 3) : 1.5f;
        c.y -= lift;
        DrawEllipse((int)c.x, (int)(c.y + rx * 0.25f), rx + 10, rx * 0.38f + 6, Fade(COL_TEXT, selected ? 0.22f : 0.12f));
    }
    if (n.state == NodeState::Current)
        DrawEllipse((int)c.x, (int)(c.y + rx * 0.2f), rx + 14 + 3 * sinf(t * 3), rx * 0.38f + 8, Fade(COL_AMBER, 0.2f + 0.08f * sinf(t * 3)));

    Color top = n.state == NodeState::Done ? COL_GREEN
              : n.state == NodeState::Current ? COL_AMBER
              : n.state == NodeState::Todo ? world_accent(n.region)
              : Color{ 58, 64, 72, 255 };
    world_platform(c, rx, top);
    switch (n.state) {
        case NodeState::Done: tick({ c.x, c.y - 1 }, 6, COL_BG); break;
        case NodeState::Current: ui_triangle({ c.x - 4, c.y - 6 }, { c.x - 4, c.y + 5 }, { c.x + 6, c.y - 0.5f }, COL_BG); break;
        case NodeState::Locked: {   // a tiny padlock
            Color lc = { 128, 136, 146, 255 };
            DrawRing({ c.x, c.y - 4 }, 2.5f, 4.2f, 180, 360, 12, lc);
            DrawRectangleRounded({ c.x - 5, c.y - 4, 10, 8 }, 0.3f, 4, lc);
            break;
        }
        default: break;
    }
}

// Text on a dark tag, so it reads over any world.
static void tag_text(const std::string &text, float x, float y, float size, Color colour, FontKind font)
{
    float w = ui_text_width(text, size, font);
    ui_panel({ x - 5, y - 2, w + 10, size * 1.25f + 4 }, Fade(COL_BG, 0.78f), Fade(COL_BG, 0.0f), 0);
    ui_text(text, x, y, size, colour, font);
}

static void draw_boss_node(const App &app, const MapNode &n, bool selected, bool hover)
{
    Vector2 c = n.pos;
    float r = 30;
    bool ready = n.state == NodeState::Current;
    bool open = n.state == NodeState::Current || n.state == NodeState::Done;   // can be fought
    bool waiting = n.state == NodeState::Todo;                                  // in an open region, topics still to clear
    Color edge = n.state == NodeState::Done ? COL_GREEN : open ? COL_CORAL : waiting ? Color{ 150, 80, 76, 255 } : COL_LINE;
    if (selected || hover) DrawPoly(c, 4, r + 9, 0, Fade(COL_TEXT, selected ? 0.16f : 0.08f));
    if (ready) DrawPoly(c, 4, r + 10, 0, Fade(COL_CORAL, 0.16f + 0.06f * sinf((float)GetTime() * 3)));
    Color fill = n.state == NodeState::Done ? ColorAlphaBlend(COL_BG, Fade(COL_GREEN, 0.18f), WHITE)
               : open ? ColorAlphaBlend(COL_BG, Fade(COL_CORAL, 0.14f), WHITE) : COL_BG;
    DrawPoly(c, 4, r, 0, fill);
    DrawPolyLinesEx(c, 4, r, 0, open ? 3 : 2, edge);
    if (n.state == NodeState::Done) tick(c, 7, COL_GREEN);
    else if (open) ui_text("!", c.x - ui_text_width("!", 20, FontKind::MonoBold) / 2, c.y - 12, 20, COL_CORAL, FontKind::MonoBold);
    else {
        // A padlock: locked until every topic in the region is cleared
        Color lc = waiting ? Color{ 200, 110, 104, 255 } : COL_MUTED;
        DrawRing({ c.x, c.y - 3 }, 4.5f, 7, 180, 360, 16, lc);
        DrawRectangleRounded({ c.x - 8, c.y - 3, 16, 12 }, 0.3f, 4, lc);
    }

    // The boss's name, above the top bosses and below the bottom ones
    const Region &reg = app.campaign[n.region];
    std::string kicker;
    Color kc = COL_MUTED;
    if (n.state == NodeState::Done) {
        const BossRecord &b = app.progress.bosses.at(reg.id);
        kicker = "BOSS - BEATEN " + std::to_string(b.seconds / 60) + ":" + (b.seconds % 60 < 10 ? "0" : "") + std::to_string(b.seconds % 60);
        kc = COL_GREEN;
    } else if (open) {
        kicker = "BOSS - READY";
        kc = COL_CORAL;
    } else if (waiting) {
        int left = region_topics_left(reg, app.progress);
        kicker = "BOSS - LOCKED, " + std::to_string(left) + (left == 1 ? " TOPIC" : " TOPICS") + " TO GO";
        kc = { 220, 120, 112, 255 };
    } else {
        kicker = "BOSS";
    }
    bool top = c.y < (NODE_TOP + NODE_BOTTOM) / 2;
    float y = top ? c.y - r - 46 : c.y + r + 8;
    tag_text(kicker, c.x - ui_text_width(kicker, 11, FontKind::Mono) / 2, y, 11, kc, FontKind::Mono);
    tag_text(reg.bossName, c.x - ui_text_width(reg.bossName, 13, FontKind::UiBold) / 2, y + 18, 13,
             (open || waiting) ? COL_TEXT : COL_MUTED, FontKind::UiBold);
}

static void draw_topic_label(const App &app, const MapNode &n)
{
    const Region &reg = app.campaign[n.region];
    float cx = column_centre(app, n.region);
    bool right = n.topic % 2 == 0;
    Color c = n.state == NodeState::Locked ? COL_MUTED : n.state == NodeState::Current ? COL_AMBER : COL_TEXT_SOFT;
    FontKind font = n.state == NodeState::Current ? FontKind::UiBold : FontKind::Ui;
    std::vector<std::string> lines = wrap(topic_title(app, reg.topics[n.topic]), LABEL_W, 12, font);
    float y = n.pos.y - lines.size() * 15 / 2.0f;
    if (n.state == NodeState::Current) {
        const char *k = "YOU ARE HERE";
        float kx = right ? cx + 64 : cx - 62 - ui_text_width(k, 10, FontKind::Mono);
        tag_text(k, kx, y - 17, 10, COL_AMBER, FontKind::Mono);
    }
    for (size_t i = 0; i < lines.size(); i++) {
        float w = ui_text_width(lines[i], 12, font);
        float x = right ? cx + 64 : cx - 62 - w;
        tag_text(lines[i], x, y + i * 17, 12, c, font);
    }
}

static void draw_bands(const App &app)
{
    const Progress &p = app.progress;
    int playing = current_region(app.campaign, p);
    float cw = column_width(app);
    for (int r = 0; r < (int)app.campaign.size(); r++) {
        const Region &reg = app.campaign[r];
        bool open = region_open(app.campaign, p, r), beaten = region_beaten(app.campaign, p, r);
        Rectangle band = { column_centre(app, r) - cw / 2 + 4, MAP_TOP + 8, cw - 8, 504 };
        // The region's world; locked ones sit behind a dark haze until they open
        world_scene(r, band, (float)GetTime());
        if (!open) DrawRectangleRec(band, Fade(COL_BG, 0.62f));
        Color edge = beaten ? COL_GREEN : (open && r == playing) ? COL_AMBER : open ? Fade(world_accent(r), 0.5f) : COL_LINE_SOFT;
        DrawRectangleRoundedLinesEx(band, 0.04f, 8, (open && r == playing) ? 2.5f : 1.5f, edge);

        std::string covers = reg.covers;
        for (char &ch : covers) ch = (char)toupper((unsigned char)ch);
        std::string state;
        Color tc = COL_MUTED;
        int done = (int)reg.topics.size() - region_topics_left(reg, p);
        std::string count = std::to_string(done) + " / " + std::to_string(reg.topics.size());
        if (beaten) { state = done == (int)reg.topics.size() ? "CLEARED" : "BOSS BEATEN, " + count; tc = COL_GREEN; }
        else if (!open) state = "LOCKED";
        else {
            state = done == (int)reg.topics.size() ? "BOSS READY" : count;
            tc = COL_AMBER;
        }
        std::string tag = covers + " - " + state;
        float cx = band.x + band.width / 2;
        ui_text(tag, cx - ui_text_width(tag, 11, FontKind::Mono) / 2, band.y + 12, 11, tc, FontKind::Mono);
        ui_text(reg.name, cx - ui_text_width(reg.name, 20, FontKind::UiBold) / 2, band.y + 30, 20,
                open ? COL_TEXT : COL_TEXT_SOFT, FontKind::UiBold);
    }
}

// ---------------------------------------------------------------------------
// The panel along the bottom: Mal on the left, the selected stop on the right
// ---------------------------------------------------------------------------

static std::string mal_line(const App &app)
{
    const Progress &p = app.progress;
    int r = app.campaignRegion;
    if (r < 0 || r >= (int)app.campaign.size()) return "";
    const Region &reg = app.campaign[r];
    if (region_beaten(app.campaign, p, r)) return reg.win;
    if (region_topics_left(reg, p) == 0)
        return reg.bossName + " is waiting. Every test you pass is a hit, but a wrong submit costs us a life. Ready?";
    return reg.story;
}

static void draw_dialogue(App &app, Rectangle box)
{
    ui_panel(box, COL_SURFACE, COL_AMBER, 2);
    mal_set_base(app.mal, MalMood::Idle);
    mal_draw(app.mal, { box.x + 50, box.y + box.height - 16 }, 60);
    ui_text("MAL", box.x + 100, box.y + 18, 12, COL_AMBER, FontKind::MonoBold);
    ui_text_wrapped(mal_line(app), box.x + 100, box.y + 38, box.width - 124, 15, COL_TEXT);
}

static void draw_card(App &app, Rectangle box)
{
    ui_panel(box, COL_SURFACE, COL_LINE, 1);
    float x = box.x + 16, y = box.y + 10, w = box.width - 32;
    if (app.campaignRegion < 0 || app.campaignRegion >= (int)app.campaign.size()) return;
    const Region &reg = app.campaign[app.campaignRegion];
    Rectangle button = { x, box.y + box.height - 10 - 36, w, 36 };

    if (app.campaignTopic < 0) {
        ui_text("Boss: " + reg.bossName, x, y, 16, COL_TEXT, FontKind::UiBold);
        y += 22;
        std::string status;
        if (progress_boss_beaten(app.progress, reg.id)) {
            int stars = progress_boss_stars(app.progress, reg.id);
            status = "Beaten, " + std::to_string(stars) + (stars == 1 ? " star" : " stars") + ". Fight again for more.";
        } else if (boss_unlocked(app.campaign, app.progress, app.campaignRegion)) {
            status = "Ready. An exam problem, against the clock.";
        } else {
            int left = region_topics_left(reg, app.progress);
            status = "Locked. Clear the " + std::to_string(left) + (left == 1 ? " topic" : " topics") + " before it to unlock the fight.";
        }
        ui_text_wrapped(status, x, y, w, 13, COL_MUTED);
        bool unlocked = boss_unlocked(app.campaign, app.progress, app.campaignRegion);
        if (ui_button(unlocked ? "Fight" : "Locked", button, unlocked ? ButtonStyle::Primary : ButtonStyle::Disabled, COL_CORAL) && unlocked)
            open_boss(app, app.campaignRegion);
        return;
    }

    const std::string &id = reg.topics[app.campaignTopic];
    int ti = topic_index(app, id);
    ui_text(topic_title(app, id), x, y, 16, COL_TEXT, FontKind::UiBold);
    y += 22;
    Mastery m = topic_mastery(app.progress, id);
    std::string status = std::string(mastery_name(m)) + "  |  " + std::to_string(topic_points(app.progress, id)) + " / 100 points";
    ui_text(status, x, y, 13, m == Mastery::NotStarted ? COL_MUTED : mastery_color(m), FontKind::Mono);
    y += 18;
    ui_text(topic_cleared(app.progress, id) ? "Cleared. Pass the quiz with all 3 lives for 100." : "Pass its quiz to clear it. All 3 lives = 100.",
            x, y, 12, COL_MUTED);

    float half = (w - 10) / 2;
    if (ui_button("Lesson", { x, button.y, half, button.height }, ButtonStyle::Primary, COL_AMBER) && ti >= 0) {
        app.selected = ti;
        open_lesson(app);
    }
    if (ui_button("Quiz", { x + half + 10, button.y, half, button.height }, ButtonStyle::Outline, COL_TEXT) && ti >= 0) {
        app.selected = ti;
        open_quiz(app);
    }
}

// ---------------------------------------------------------------------------
// Mal hopping between stops, and bursts when something is cleared or opens
// ---------------------------------------------------------------------------

static Vector2 malFrom, malTo;
static float hopT = 1;
static bool malPlaced = false;

// Where Mal is this frame, hopping in an arc towards `target`.
static Vector2 mal_hop(App &app, Vector2 target)
{
    if (!malPlaced) { malFrom = malTo = target; hopT = 1; malPlaced = true; }
    if (target.x != malTo.x || target.y != malTo.y) {   // a new stop: jump from wherever Mal is now
        float u = fminf(1.0f, hopT);
        malFrom = { malFrom.x + (malTo.x - malFrom.x) * u, malFrom.y + (malTo.y - malFrom.y) * u };
        malTo = target;
        hopT = 0;
    }
    if (hopT < 1) {
        float dist = sqrtf((malTo.x - malFrom.x) * (malTo.x - malFrom.x) + (malTo.y - malFrom.y) * (malTo.y - malFrom.y));
        hopT += GetFrameTime() / fminf(0.7f, 0.25f + dist / 1200.0f);   // longer hops take a little longer
        if (hopT >= 1) { hopT = 1; app.mal.squash = 0.72f; }          // land with a squash
    }
    float u = fminf(1.0f, hopT), e = u * u * (3 - 2 * u);
    float arc = sinf(u * PI) * (40 + 0.1f * fabsf(malTo.x - malFrom.x));
    return { malFrom.x + (malTo.x - malFrom.x) * e, malFrom.y + (malTo.y - malFrom.y) * e - arc };
}

static void boss_fell(int region);

// Compare every stop with how it looked last time: a topic just cleared gets
// a burst and "CLEARED!", and a region or boss that just opened gets "UNLOCKED!".
static void celebrate_changes(App &app, const std::vector<MapNode> &nodes)
{
    static std::vector<NodeState> last;
    if (last.size() != nodes.size()) { last.clear(); for (const MapNode &n : nodes) last.push_back(n.state); return; }
    bool movedOn = false;   // the selected stop was just cleared: Mal hops to the next one
    for (size_t i = 0; i < nodes.size(); i++) {
        NodeState was = last[i], now = nodes[i].state;
        Vector2 at = nodes[i].pos;
        if (now == NodeState::Done && was != NodeState::Done) {
            fx_burst(at, 34, COL_GREEN, COL_AMBER);
            fx_float_text(nodes[i].topic < 0 ? "BEATEN!" : "CLEARED!", { at.x, at.y - 60 }, 22, COL_GREEN);
            sfx_play(Sfx::Combo);
            mal_react(app.mal, MalMood::Cheer, 1.8f);
            if (is_selected(app, nodes[i])) movedOn = true;
            if (nodes[i].topic < 0) boss_fell(nodes[i].region);   // fly out to the city and over to the next island
        } else if (was == NodeState::Locked && now != NodeState::Locked) {
            fx_burst(at, 24, world_accent(nodes[i].region), COL_AMBER);
            fx_float_text("UNLOCKED!", { at.x, at.y - 60 }, 20, COL_AMBER);
        } else if (nodes[i].topic < 0 && was == NodeState::Todo && now == NodeState::Current) {
            fx_burst(at, 30, COL_CORAL, COL_AMBER);
            fx_float_text("BOSS READY!", { at.x, at.y - 70 }, 20, COL_CORAL);
            fx_shake(0.3f);
        }
        last[i] = now;
    }
    if (movedOn)
        for (const MapNode &n : nodes)
            if (n.state == NodeState::Current) { app.campaignRegion = n.region; app.campaignTopic = n.topic; break; }
}

// ---------------------------------------------------------------------------

void open_campaign(App &app)
{
    app.inCampaign = true;
    app.screen = Screen::Campaign;
    if (app.campaign.empty()) return;
    // Start on whatever is next to play
    int r = current_region(app.campaign, app.progress);
    app.campaignRegion = r;
    app.campaignTopic = next_topic(app.campaign[r], app.progress);
    if (app.mal.mood == MalMood::Freed) mal_react(app.mal, MalMood::Idle, 0);
}

static void select_topic_for_lesson(App &app)
{
    if (app.campaignTopic < 0) return;
    const Region &reg = app.campaign[app.campaignRegion];
    int ti = topic_index(app, reg.topics[app.campaignTopic]);
    if (ti >= 0) app.selected = ti;
}

// ---------------------------------------------------------------------------
// The 2D world map (the campaign as it always was)
// ---------------------------------------------------------------------------

// Keys on the 2D map. Returns true if the screen changed.
static bool world_keys(App &app, const std::vector<MapNode> &nodes)
{
    std::vector<int> open;
    int at = -1;
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (nodes[i].state == NodeState::Locked) continue;
        if (is_selected(app, nodes[i])) at = (int)open.size();
        open.push_back(i);
    }
    int move = (IsKeyPressed(KEY_RIGHT) ? 1 : 0) - (IsKeyPressed(KEY_LEFT) ? 1 : 0);
    if (move != 0 && !open.empty()) {
        at = at < 0 ? 0 : (at + move + (int)open.size()) % (int)open.size();
        app.campaignRegion = nodes[open[at]].region;
        app.campaignTopic = nodes[open[at]].topic;
        sfx_play(Sfx::Click);
    }
    if (IsKeyPressed(KEY_ENTER)) {
        if (app.campaignTopic < 0) { open_boss(app, app.campaignRegion); return app.screen != Screen::Campaign; }   // does nothing while it's locked
        select_topic_for_lesson(app);
        open_lesson(app);
        return true;
    }
    if (IsKeyPressed(KEY_Q) && app.campaignTopic >= 0) {
        select_topic_for_lesson(app);
        open_quiz(app);
        return true;
    }
    return false;
}

// Draw the 2D map. live = it takes clicks (not while zooming in or out).
// Returns true if a click left the screen.
static bool draw_world(App &app, const std::vector<MapNode> &nodes, bool live)
{
    draw_bands(app);

    // The path: green between stops that are done (and on to the one being
    // played), dotted everywhere else
    for (int i = 0; i + 1 < (int)nodes.size(); i++) {
        bool done = nodes[i].state == NodeState::Done &&
                    (nodes[i + 1].state == NodeState::Done || nodes[i + 1].state == NodeState::Current);
        Vector2 a = { nodes[i].pos.x, nodes[i].pos.y + 6 }, b = { nodes[i + 1].pos.x, nodes[i + 1].pos.y + 6 };
        bool chains = nodes[i].region == nodes[i + 1].region && world_has_chains(nodes[i].region);
        DrawLineEx(a, b, 10, Fade(COL_BG, 0.55f));   // the road
        if (chains) world_chain(a, b, done ? COL_GREEN : Color{ 96, 112, 130, 255 });   // the islands: chains of links
        else if (done) DrawLineEx(a, b, 4, COL_GREEN);
        else dotted_line(a, b, Fade(COL_TEXT, 0.35f));
    }

    // The stops, and clicking them
    Vector2 mouse = GetMousePosition();
    for (const MapNode &n : nodes) {
        float hitR = n.topic < 0 ? 32 : 24;
        bool hover = live && n.state != NodeState::Locked &&
                     (fabsf(mouse.x - n.pos.x) < hitR && fabsf(mouse.y - n.pos.y) < hitR);
        if (hover) {
            ui_want_hand();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                bool again = is_selected(app, n);
                app.campaignRegion = n.region;
                app.campaignTopic = n.topic;
                sfx_play(Sfx::Click);
                if (n.topic < 0 && again && boss_unlocked(app.campaign, app.progress, n.region)) { open_boss(app, n.region); return true; }   // a second click on a ready boss fights it
            }
        }
        if (n.topic < 0) draw_boss_node(app, n, is_selected(app, n), hover);
        else {
            draw_topic_node(n, is_selected(app, n), hover);
            draw_topic_label(app, n);
        }
    }

    if (live) celebrate_changes(app, nodes);

    // Mal stands on the selected stop, and hops along when it changes
    for (const MapNode &n : nodes)
        if (is_selected(app, n)) {
            Vector2 feet = n.topic < 0 ? Vector2{ n.pos.x, n.pos.y - 30 } : Vector2{ n.pos.x, n.pos.y + 2 };
            mal_draw_with_pet(app.mal, mal_hop(app, feet), 40);
        }

    // ----- Bottom: Mal's dialogue and the selected stop -----
    float by = MAP_TOP + 520;
    draw_dialogue(app, { MARGIN - 8, by, SCREEN_W - 2 * MARGIN - 330, SCREEN_H - by - 14 });
    if (live) draw_card(app, { SCREEN_W - MARGIN - 314, by, 322, SCREEN_H - by - 14 });
    else ui_panel({ SCREEN_W - MARGIN - 314, by, 322, SCREEN_H - by - 14 }, COL_SURFACE, COL_LINE, 1);
    return app.screen != Screen::Campaign;
}

// ---------------------------------------------------------------------------
// Memory City, and zooming between it and the 2D map
// ---------------------------------------------------------------------------

enum class View { World, ToCity, City, ToWorld };
static View view = View::World;
static float viewT = 0;           // 0..1 through a zoom
static int diveRegion = 0;        // the island being zoomed out of, or into
static CityCam ccam;
static int celebrateRegion = -1;  // a boss just fell here...
static float celebrateIn = 0;     // ...and in this long, fly out to show the next island open up
static float cityHold = 0;        // the fly-by: how long to look before diving into the next island
static int glowBridge = -1;
static Vector2 pressAt;
static bool dragging = false;

static void boss_fell(int region) { celebrateRegion = region; celebrateIn = 1.8f; }

static void zoom_out(App &app)
{
    view = View::ToCity;
    viewT = 0;
    diveRegion = app.campaignRegion;
    ccam.fovy = 26;
    ccam.target = city_centre((int)app.campaign.size());
    sfx_play(Sfx::Click);
}

// Dive into a region; topic = the stop to select there (-2 = whatever's next).
static void dive_in(App &app, int region, int topic)
{
    view = View::ToWorld;
    viewT = 0;
    diveRegion = region;
    app.campaignRegion = region;
    if (topic == -2) {
        int next = next_topic(app.campaign[region], app.progress);
        topic = next;   // -1 (the boss) once every topic is cleared
    }
    app.campaignTopic = topic;
    sfx_play(Sfx::Click);
}

static std::vector<CityRegion> city_regions(const App &app) { return city_regions_for(app.campaign, app.progress); }

static std::vector<CityNode> city_nodes(const std::vector<MapNode> &nodes)
{
    std::vector<CityNode> out;
    for (const MapNode &n : nodes) out.push_back({ n.region, n.topic, (int)n.state });
    return out;
}

static float ease(float u) { u = fmaxf(0, fminf(1, u)); return u * u * (3 - 2 * u); }

// The 2D map, zoomed in `scale` times around a region's column.
static void draw_world_zoomed(App &app, const std::vector<MapNode> &nodes, int region, float scale)
{
    Camera2D fit = ui_get_fit();
    Vector2 anchor = { column_centre(app, region), MAP_TOP + 260 };
    Camera2D z = fit;
    z.target = anchor;
    z.offset = { fit.offset.x + fit.zoom * anchor.x, fit.offset.y + fit.zoom * anchor.y };
    z.zoom = fit.zoom * scale;
    EndMode2D();
    ui_set_fit(z);
    BeginMode2D(z);
    draw_world(app, nodes, false);
    EndMode2D();
    ui_set_fit(fit);
    BeginMode2D(fit);
}

static void dark(float a) { DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade({ 6, 8, 14, 255 }, fmaxf(0, fminf(1, a)))); }

// A tag over the city: a name and a line under it.
static void city_label(Vector2 at, const std::string &name, const std::string &sub, Color subColour)
{
    float nw = ui_text_width(name, 16, FontKind::UiBold), sw = ui_text_width(sub, 11, FontKind::Mono);
    float w = fmaxf(nw, sw) + 20;
    ui_panel({ at.x - w / 2, at.y - 4, w, 44 }, Fade(COL_BG, 0.82f), Fade(COL_LINE, 0.6f), 1);
    ui_text(name, at.x - nw / 2, at.y + 2, 16, COL_TEXT, FontKind::UiBold);
    ui_text(sub, at.x - sw / 2, at.y + 23, 11, subColour, FontKind::Mono);
}

// The city when it's settled: turning, zooming, hovering, clicking.
static void city_frame(App &app, const std::vector<MapNode> &nodes, float t)
{
    int rc = (int)app.campaign.size();
    std::vector<CityRegion> regions = city_regions(app);
    std::vector<CityNode> cn = city_nodes(nodes);
    Vector2 mouse = GetMousePosition();
    float ppu = SCREEN_H / ccam.fovy;   // pixels per world unit, at this zoom

    // What's under the mouse: a pad first, then an island
    int hoverNode = -1, hoverRegion = -1;
    bool overUi = mouse.y < HEADER_H || mouse.y > SCREEN_H - 56;
    if (!overUi && !dragging) {
        float best = 1e9f;
        for (int i = 0; i < (int)cn.size(); i++) {
            if (cn[i].state == 3) continue;
            Vector2 sp = city_to_screen(city_node(cn, i, rc), ccam);
            float d = sqrtf((mouse.x - sp.x) * (mouse.x - sp.x) + (mouse.y - sp.y) * (mouse.y - sp.y));
            float hit = (cn[i].topic < 0 ? 1.1f : 0.8f) * ppu + 4;
            if (d < hit && d < best) { best = d; hoverNode = i; }
        }
        if (hoverNode >= 0) hoverRegion = cn[hoverNode].region;
        else
            for (int r = 0; r < rc; r++) {
                Vector2 sp = city_to_screen(city_island(r, rc), ccam);
                float dx = (mouse.x - sp.x) / (3.9f * ppu), dy = (mouse.y - sp.y) / (2.6f * ppu);
                if (dx * dx + dy * dy < 1) { hoverRegion = r; break; }
            }
    }

    // Turning (drag), zooming (scroll, towards the mouse), clicking
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { pressAt = mouse; dragging = false; }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 d = GetMouseDelta();
        if (fabsf(mouse.x - pressAt.x) + fabsf(mouse.y - pressAt.y) > 6) dragging = true;
        if (dragging) ccam.yaw += d.x * 0.008f;
    }
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && !overUi) {
        float old = ccam.fovy;
        ccam.fovy = fmaxf(11.0f, fminf(34.0f, ccam.fovy * (1 - wheel * 0.12f)));
        // Keep the spot under the mouse where it is: move the target towards it
        Vector2 c = city_to_screen(ccam.target, ccam);
        float k = 1 - ccam.fovy / old;
        float ux = (mouse.x - c.x) / ppu, uy = (mouse.y - c.y) / ppu;   // screen offset in world units
        // On the ground, screen-right is (sin, -cos) and screen-down is towards the camera,
        // stretched because the ground is seen at a slant (the camera's pitch is 0.6)
        float down = uy / sinf(0.6f);
        ccam.target.x += (ux * sinf(ccam.yaw) + down * cosf(ccam.yaw)) * k;
        ccam.target.z += (-ux * cosf(ccam.yaw) + down * sinf(ccam.yaw)) * k;
        if (ccam.fovy >= 33.9f) ccam.target = city_centre(rc);
        ccam.target.x = fmaxf(-14, fminf(14, ccam.target.x));
        ccam.target.z = fmaxf(-8, fminf(8, ccam.target.z));
    }
    bool click = IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !dragging && !overUi;
    if (hoverNode >= 0 || hoverRegion >= 0) ui_want_hand();
    if (cityHold > 0) {   // the fly-by after a boss: look, then dive into the next island
        cityHold -= GetFrameTime();
        if (cityHold <= 0 || click) {
            glowBridge = -1;
            dive_in(app, celebrateRegion + 1 < rc ? celebrateRegion + 1 : rc - 1, -2);
            celebrateRegion = -1;
            return;
        }
    } else if (click && hoverNode >= 0) {
        dive_in(app, cn[hoverNode].region, cn[hoverNode].topic);
    } else if (click && hoverRegion >= 0) {
        if (regions[hoverRegion].open) dive_in(app, hoverRegion, hoverRegion == current_region(app.campaign, app.progress) ? -2 : 0);
        else {
            Vector2 sp = city_to_screen(city_island(hoverRegion, rc), ccam);
            fx_float_text("LOCKED: beat the boss before it", { sp.x, sp.y - 40 }, 18, COL_CORAL);
            sfx_play(Sfx::Wrong);
            fx_shake(0.2f);
        }
    }

    city_draw(regions, cn, ccam, t, hoverRegion, hoverNode, glowBridge);

    // Mal on the selected pad (beside it, for a boss)
    for (int i = 0; i < (int)cn.size(); i++)
        if (cn[i].region == app.campaignRegion && cn[i].topic == app.campaignTopic) {
            Vector3 p = city_node(cn, i, rc);
            if (cn[i].topic < 0) { p.x += cosf(ccam.yaw) * 1.3f; p.z += sinf(ccam.yaw) * 1.3f; }
            Vector2 sp = city_to_screen({ p.x, p.y + 0.22f, p.z }, ccam);
            mal_set_base(app.mal, MalMood::Idle);
            mal_draw_with_pet(app.mal, sp, fminf(70.0f, 34 * 26 / ccam.fovy));
        }

    // Island names, and what the mouse is over
    for (int r = 0; r < rc; r++) {
        Vector3 c = city_island(r, rc);
        // Islands nearer the camera get their name underneath, ones further back get it above,
        // so a name never lands on the island in front
        Vector3 mid = city_centre(rc);
        bool front = (c.x - mid.x) * cosf(ccam.yaw) + (c.z - mid.z) * sinf(ccam.yaw) > 0;
        Vector2 sp = city_to_screen({ c.x, front ? c.y - 4.4f : c.y + 5.2f, c.z }, ccam);
        const CityRegion &R = regions[r];
        Color sc = R.beaten ? COL_GREEN : !R.open ? COL_MUTED : R.status == "BOSS READY" ? COL_CORAL : COL_AMBER;
        city_label(sp, R.name, R.status, sc);
    }
    if (hoverNode >= 0) {
        const Region &reg = app.campaign[cn[hoverNode].region];
        std::string title = cn[hoverNode].topic < 0 ? "Boss: " + reg.bossName : topic_title(app, reg.topics[cn[hoverNode].topic]);
        std::string sub = cn[hoverNode].topic < 0 ? (cn[hoverNode].state == 0 ? "beaten" : cn[hoverNode].state == 1 ? "ready to fight" : "locked")
                        : mastery_name(topic_mastery(app.progress, reg.topics[cn[hoverNode].topic]));
        float w = fmaxf(ui_text_width(title, 14, FontKind::UiBold), ui_text_width(sub, 11, FontKind::Mono)) + 20;
        float x = fminf(mouse.x + 16, SCREEN_W - w - 8), y = mouse.y + 14;
        ui_panel({ x, y, w, 42 }, COL_SURFACE, COL_AMBER, 1);
        ui_text(title, x + 10, y + 5, 14, COL_TEXT, FontKind::UiBold);
        ui_text(sub, x + 10, y + 24, 11, COL_AMBER, FontKind::Mono);
    }
    if (cityHold > 0 && celebrateRegion >= 0 && celebrateRegion + 1 < rc) {
        Vector3 c = city_island(celebrateRegion + 1, rc);
        Vector2 sp = city_to_screen({ c.x, c.y + 4.0f, c.z }, ccam);
        float k = fmodf(t * 2, 1.0f);
        ui_text("UNLOCKED!", sp.x - ui_text_width("UNLOCKED!", 26, FontKind::MonoBold) / 2, sp.y - 20 - k * 6, 26, COL_AMBER, FontKind::MonoBold);
    }

    // Hints along the bottom
    DrawRectangle(0, SCREEN_H - 44, SCREEN_W, 44, Fade(COL_BG, 0.85f));
    ui_text("MEMORY CITY", MARGIN, SCREEN_H - 30, 13, COL_AMBER, FontKind::MonoBold);
    ui_text("drag to turn      scroll to zoom      click an island or a stop to dive in      M or ESC: back to the map",
            MARGIN + 130, SCREEN_H - 30, 13, COL_MUTED, FontKind::Mono);
}

// ---------------------------------------------------------------------------

void screen_campaign(App &app)
{
    float t = (float)GetTime(), dt = GetFrameTime();
    bool settled = view == View::World || view == View::City;
    if (IsKeyPressed(KEY_ESCAPE) && settled) {
        if (view == View::City) dive_in(app, app.campaignRegion, app.campaignTopic);
        else { app.inCampaign = false; app.screen = Screen::Title; return; }
    }

    if (app.campaign.empty()) {
        ui_header("Campaign  >  a journey through memory");
        ui_text("The campaign didn't load.", MARGIN, HEADER_H + 40, 22, COL_CORAL, FontKind::UiBold);
        ui_text_wrapped(app.campaignError, MARGIN, HEADER_H + 80, SCREEN_W - 2 * MARGIN, 15, COL_TEXT_SOFT, FontKind::Mono);
        return;
    }
    if (diveRegion >= (int)app.campaign.size()) diveRegion = 0;

    std::vector<MapNode> nodes = build_nodes(app);
    int rc = (int)app.campaign.size();

    switch (view) {
        case View::World: {
            if (world_keys(app, nodes)) return;
            if (IsKeyPressed(KEY_M) || GetMouseWheelMove() < 0) { zoom_out(app); break; }
            if (celebrateIn > 0) {   // a boss just fell: fly out to watch the next island open
                celebrateIn -= dt;
                if (celebrateIn <= 0 && celebrateRegion >= 0 && celebrateRegion + 1 < rc) {
                    zoom_out(app);
                    diveRegion = celebrateRegion;
                    glowBridge = celebrateRegion;
                    cityHold = 2.8f;
                }
            }
            if (draw_world(app, nodes, true)) return;
            break;
        }
        case View::ToCity: {
            viewT += dt / 1.0f;
            if (viewT < 0.4f) {   // the map zooms in on the column, and darkens
                float u = viewT / 0.4f;
                draw_world_zoomed(app, nodes, diveRegion, 1 + 1.4f * u * u);
                dark(u);
            } else {              // the city pulls back from that island
                float u = ease((viewT - 0.4f) / 0.6f);
                Vector3 is = city_island(diveRegion, rc);
                CityCam c = ccam;
                c.target = { is.x + (ccam.target.x - is.x) * u, is.y + (ccam.target.y - is.y) * u, is.z + (ccam.target.z - is.z) * u };
                c.fovy = 6 + (ccam.fovy - 6) * u;
                city_draw(city_regions(app), city_nodes(nodes), c, t, -1, -1, glowBridge);
                dark(1 - (viewT - 0.4f) / 0.35f);
            }
            if (viewT >= 1) {
                view = View::City;
                if (cityHold > 0) { sfx_play(Sfx::LevelUp); mal_react(app.mal, MalMood::Cheer, 2.5f); }
            }
            break;
        }
        case View::City: {
            if (IsKeyPressed(KEY_M)) { dive_in(app, app.campaignRegion, app.campaignTopic); break; }
            city_frame(app, nodes, t);
            break;
        }
        case View::ToWorld: {
            viewT += dt / 1.0f;
            if (viewT < 0.6f) {   // the city flies into the island, and darkens
                float u = ease(viewT / 0.6f);
                Vector3 is = city_island(diveRegion, rc);
                CityCam c = ccam;
                c.target = { ccam.target.x + (is.x - ccam.target.x) * u, ccam.target.y + (is.y - ccam.target.y) * u, ccam.target.z + (is.z - ccam.target.z) * u };
                c.fovy = ccam.fovy + (5 - ccam.fovy) * u;
                city_draw(city_regions(app), city_nodes(nodes), c, t, -1, -1, glowBridge);
                dark((viewT - 0.25f) / 0.35f);
            } else {              // and comes out of the dark in the 2D world, zooming back to normal
                float u = (viewT - 0.6f) / 0.4f;
                draw_world_zoomed(app, nodes, diveRegion, 1 + 1.4f * (1 - ease(u)) * (1 - ease(u)));
                dark(1 - u * 1.4f);
            }
            if (viewT >= 1) { view = View::World; malPlaced = false; }
            break;
        }
    }

    // The header goes on top of everything
    ui_header(view == View::City ? "Campaign  >  Memory City" : "Campaign  >  a journey through memory");
    if (ui_button("Topic map", { SCREEN_W - MARGIN - 120, 12, 120, 40 }, ButtonStyle::Outline, COL_GREEN)) {
        app.inCampaign = false;
        app.screen = Screen::Map;
        view = View::World;
        return;
    }
    Rectangle cityButton = { SCREEN_W - MARGIN - 120 - 12 - 132, 12, 132, 40 };
    if (view == View::City) {
        if (ui_button("Back to map", cityButton, ButtonStyle::Primary, COL_AMBER)) dive_in(app, app.campaignRegion, app.campaignTopic);
    } else if (ui_button("3D city", cityButton, ButtonStyle::Outline, COL_BLUE) && view == View::World) {
        zoom_out(app);
    }
    int xp = progress_xp(app.progress);
    ui_level_badge(cityButton.x - 24 - ui_level_badge_width(xp), 15, xp);
}
