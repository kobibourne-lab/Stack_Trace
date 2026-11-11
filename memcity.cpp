// memcity.cpp - drawing Memory City, the campaign in 3D (see memcity.h).
//
// The city is drawn into an off-screen image twice the size of the screen,
// then shrunk onto it, which smooths the edges of every block. Blocks are
// drawn face by face with each face a different shade, so they look solid
// without needing any lighting.

#include "memcity.h"
#include "theme.h"
#include "ui.h"
#include "rlgl.h"
#include <cmath>

static RenderTexture2D target = {};
static const int SS = 2;   // drawn at twice the size, then shrunk: smooth edges

// ---------------------------------------------------------------------------
// Where things are
// ---------------------------------------------------------------------------

static Vector3 v3(CityPoint p) { return { p.x, p.y, p.z }; }

Vector3 city_island(int region, int regionCount) { return v3(city_island_at(region, regionCount)); }
Vector3 city_centre(int regionCount) { return v3(city_centre_at(regionCount)); }
Vector3 city_node(const std::vector<CityNode> &nodes, int index, int regionCount) { return v3(city_node_at(nodes, index, regionCount)); }

static Camera3D make_camera(const CityCam &cc)
{
    const float pitch = 0.6f, dist = 44;
    Camera3D cam = {};
    cam.target = cc.target;
    cam.position = { cc.target.x + cosf(cc.yaw) * cosf(pitch) * dist, cc.target.y + sinf(pitch) * dist,
                     cc.target.z + sinf(cc.yaw) * cosf(pitch) * dist };
    cam.up = { 0, 1, 0 };
    cam.fovy = cc.fovy;
    cam.projection = CAMERA_ORTHOGRAPHIC;
    return cam;
}

Vector2 city_to_screen(Vector3 p, const CityCam &cam)
{
    Vector2 s = GetWorldToScreenEx(p, make_camera(cam), SCREEN_W * SS, SCREEN_H * SS);
    return { s.x / SS, s.y / SS };
}

void city_unload()
{
    if (target.id != 0) UnloadRenderTexture(target);
    target = {};
}

// ---------------------------------------------------------------------------
// Pieces
// ---------------------------------------------------------------------------

static Color shade(Color c, float f)
{
    auto ch = [&](unsigned char v) { float x = v * f; return (unsigned char)(x > 255 ? 255 : x); };
    return { ch(c.r), ch(c.g), ch(c.b), c.a };
}

// A box with each face its own shade, so it reads as solid.
static void box(Vector3 c, Vector3 s, Color col)
{
    float x0 = c.x - s.x / 2, x1 = c.x + s.x / 2, y0 = c.y - s.y / 2, y1 = c.y + s.y / 2, z0 = c.z - s.z / 2, z1 = c.z + s.z / 2;
    auto quad = [&](float f, Vector3 a, Vector3 b, Vector3 cc, Vector3 d) {
        Color k = shade(col, f);
        rlColor4ub(k.r, k.g, k.b, k.a);
        rlVertex3f(a.x, a.y, a.z); rlVertex3f(b.x, b.y, b.z); rlVertex3f(cc.x, cc.y, cc.z); rlVertex3f(d.x, d.y, d.z);
    };
    rlBegin(RL_QUADS);
    quad(1.0f, { x0, y1, z0 }, { x0, y1, z1 }, { x1, y1, z1 }, { x1, y1, z0 });    // top
    quad(0.62f, { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { x1, y0, z1 });   // +x
    quad(0.8f, { x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 });    // +z
    quad(0.7f, { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 }, { x1, y0, z0 });    // -z
    quad(0.55f, { x0, y0, z0 }, { x0, y0, z1 }, { x0, y1, z1 }, { x0, y1, z0 });   // -x
    quad(0.4f, { x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 });    // bottom
    rlEnd();
}

// A pointer: a tube arcing from a to b, with an arrowhead at the end.
static void arc(Vector3 a, Vector3 b, float height, float r, Color col, bool head)
{
    const int n = 22;
    Vector3 prev = a;
    for (int i = 1; i <= n; i++) {
        float t = (float)i / n;
        Vector3 p = { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t + sinf(t * PI) * height, a.z + (b.z - a.z) * t };
        if (head && i == n) DrawCylinderEx(prev, p, r * 3.2f, 0.0f, 8, col);
        else DrawCylinderEx(prev, p, r, r, 6, col);
        prev = p;
    }
}

static Vector3 arc_point(Vector3 a, Vector3 b, float height, float t)
{
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t + sinf(t * PI) * height, a.z + (b.z - a.z) * t };
}

static Color world_top(int w)
{
    static const Color tops[4] = { { 70, 160, 90, 255 }, { 120, 90, 190, 255 }, { 60, 120, 200, 255 }, { 190, 140, 70, 255 } };
    return tops[w % 4];
}

static Color world_dirt(int w)
{
    static const Color dirt[4] = { { 60, 50, 40, 255 }, { 55, 45, 70, 255 }, { 40, 50, 70, 255 }, { 70, 55, 40, 255 } };
    return dirt[w % 4];
}

// The island itself: a grassy slab over layers of rock that taper away below.
static void island(Vector3 c, int w, float bright)
{
    box({ c.x, c.y - 0.5f, c.z }, { 7, 1, 7 }, shade(world_top(w), bright));
    box({ c.x, c.y - 1.6f, c.z }, { 6.2f, 1.2f, 6.2f }, shade(world_dirt(w), bright));
    box({ c.x, c.y - 2.7f, c.z }, { 4.6f, 1.0f, 4.6f }, shade(world_dirt(w), bright * 0.85f));
    box({ c.x, c.y - 3.5f, c.z }, { 2.6f, 0.7f, 2.6f }, shade(world_dirt(w), bright * 0.7f));
}

// What stands in the middle of each island.
static void decor(Vector3 c, int w, float t, float bright)
{
    switch (w % 4) {
        case 0: {   // the Stack: a tower of call frames, the newest on top
            for (int i = 0; i < 5; i++) {
                float s = 2.6f - i * 0.36f;
                Color col = i == 4 ? COL_AMBER : (i % 2 ? Color{ 150, 220, 160, 255 } : Color{ 110, 200, 130, 255 });
                box({ c.x, c.y + 0.3f + i * 0.6f, c.z }, { s, 0.52f, s }, shade(col, bright));
            }
            break;
        }
        case 1: {   // Address Space: a field of memory cells, one pointing at another
            for (int x = 0; x < 3; x++)
                for (int z = 0; z < 3; z++) {
                    float h = 0.6f + ((x + z) % 2) * 0.4f;
                    Color col = (x == 2 && z == 0) ? COL_AMBER : Color{ 170, 140, 230, 255 };
                    box({ c.x - 1.15f + x * 1.15f, c.y + h / 2, c.z - 1.15f + z * 1.15f }, { 0.9f, h, 0.9f }, shade(col, bright));
                }
            arc({ c.x - 1.15f, c.y + 1.0f, c.z + 1.15f }, { c.x + 1.15f, c.y + 1.1f, c.z - 1.15f }, 1.6f, 0.06f, shade(COL_AMBER, bright), true);
            break;
        }
        case 2: {   // the Heap: blocks dropped anywhere, and one leaking
            const float hb[5][3] = { { -1.0f, -0.6f, 1.2f }, { 0.3f, -1.0f, 1.7f }, { 1.0f, 0.4f, 0.9f }, { -0.6f, 0.9f, 2.0f }, { 0.7f, 1.2f, 0.7f } };
            for (int i = 0; i < 5; i++)
                box({ c.x + hb[i][0], c.y + hb[i][2] / 2, c.z + hb[i][1] }, { 0.95f, hb[i][2], 0.95f },
                    shade(i == 3 ? COL_CORAL : Color{ 130, 185, 240, 255 }, bright));
            float pool = 0.85f + 0.08f * sinf(t * 2);
            DrawCylinder({ c.x - 0.6f, c.y + 0.01f, c.z + 1.6f }, pool, pool, 0.04f, 18, Fade(shade({ 140, 210, 90, 255 }, bright), 0.85f));
            break;
        }
        default: {   // Object Land: buildings (classes), windows lit
            const float bl[4][3] = { { -0.8f, -0.8f, 2.6f }, { 0.7f, -0.7f, 3.6f }, { -0.7f, 0.8f, 1.8f }, { 0.8f, 0.7f, 2.8f } };
            for (int i = 0; i < 4; i++) {
                box({ c.x + bl[i][0], c.y + bl[i][2] / 2, c.z + bl[i][1] }, { 1.15f, bl[i][2], 1.15f }, shade({ 120, 92, 60, 255 }, bright));
                for (int f = 0; f < (int)(bl[i][2] / 0.55f) - 1; f++) {
                    bool lit = ((i * 7 + f * 3 + (int)(t * 0.5f)) % 5) != 0;
                    box({ c.x + bl[i][0] + 0.585f, c.y + 0.45f + f * 0.55f, c.z + bl[i][1] }, { 0.02f, 0.22f, 0.7f },
                        Fade(shade({ 255, 210, 120, 255 }, bright), lit ? 0.8f : 0.15f));
                    box({ c.x + bl[i][0], c.y + 0.45f + f * 0.55f, c.z + bl[i][1] + 0.585f }, { 0.7f, 0.22f, 0.02f },
                        Fade(shade({ 255, 210, 120, 255 }, bright), lit ? 0.6f : 0.12f));
                }
            }
            break;
        }
    }
}

static void pad(Vector3 p, const CityNode &n, int region, bool hover, float t)
{
    Color c = n.state == 0 ? COL_GREEN : n.state == 1 ? COL_AMBER : n.state == 2 ? shade(world_top(region), 1.35f) : Color{ 80, 88, 104, 255 };
    float lift = hover ? 0.18f : 0.0f;
    if (n.state == 1) lift += 0.08f + 0.06f * sinf(t * 4);
    p.y += lift;
    if (n.topic < 0) {   // a boss: a block waiting on a bigger pad
        Color boss = n.state == 0 ? Color{ 110, 200, 120, 255 } : n.state == 3 ? Color{ 90, 92, 100, 255 } : COL_CORAL;
        DrawCylinder(p, 0.9f, 0.9f, 0.2f, 24, shade(boss, 0.6f));
        float bob = n.state == 1 ? 0.1f * sinf(t * 3) : 0;
        box({ p.x, p.y + 0.85f + bob, p.z }, { 1.2f, 1.2f, 1.2f }, boss);
        if (n.state == 1 || hover) DrawCylinderWires(p, 1.25f, 1.25f, 0.02f, 28, Fade(COL_CORAL, 0.8f));
        if (n.state == 0) {   // a green flag: beaten
            DrawCylinderEx({ p.x + 0.7f, p.y, p.z }, { p.x + 0.7f, p.y + 2.2f, p.z }, 0.04f, 0.04f, 6, COL_TEXT);
            box({ p.x + 1.05f, p.y + 1.95f + 0.05f * sinf(t * 5), p.z }, { 0.7f, 0.4f, 0.03f }, COL_GREEN);
        }
        return;
    }
    DrawCylinder(p, 0.6f, 0.6f, 0.22f, 20, c);
    DrawCylinder({ p.x, p.y + 0.22f, p.z }, 0.48f, 0.48f, 0.02f, 20, shade(c, 1.2f));
    if (n.state <= 1 || hover)
        DrawCylinderWires(p, 0.85f + (n.state == 1 ? 0.1f * sinf(t * 4) : 0), 0.85f, 0.02f, 24, Fade(hover ? COL_TEXT : c, 0.7f));
}

// ---------------------------------------------------------------------------

void city_draw(const std::vector<CityRegion> &regions, const std::vector<CityNode> &nodes, const CityCam &cc,
               float t, int hoverRegion, int hoverNode, int glowBridge)
{
    int rc = (int)regions.size();
    if (target.id == 0) {
        target = LoadRenderTexture(SCREEN_W * SS, SCREEN_H * SS);
        SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    }

    // The sky, drawn straight onto the screen
    DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, { 18, 22, 42, 255 }, { 8, 10, 16, 255 });
    for (int i = 0; i < 110; i++) {
        float tw = 0.25f + 0.35f * (0.5f + 0.5f * sinf(t * (0.6f + (i % 5) * 0.3f) + i));
        DrawCircleV({ (float)((i * 197) % SCREEN_W), (float)((i * 113) % 560) }, (i % 3) ? 1.0f : 1.6f, Fade(WHITE, tw));
    }

    // The city, into the off-screen image (this leaves the app's 2D camera, so it's put back after)
    Camera3D cam = make_camera(cc);
    Camera2D fit = ui_get_fit();
    EndMode2D();
    BeginTextureMode(target);
    ClearBackground({ 0, 0, 0, 0 });
    BeginMode3D(cam);
    rlDisableBackfaceCulling();

    for (int i = -36; i <= 36; i += 2) {   // the memory grid far below
        DrawLine3D({ (float)i, -7, -36 }, { (float)i, -7, 36 }, Fade({ 60, 90, 160, 255 }, 0.22f));
        DrawLine3D({ -36, -7, (float)i }, { 36, -7, (float)i }, Fade({ 60, 90, 160, 255 }, 0.22f));
    }

    for (int r = 0; r < rc; r++) {
        Vector3 c = city_island(r, rc);
        const CityRegion &R = regions[r];
        float bright = R.open ? (r == hoverRegion ? 1.15f : 1.0f) : (r == hoverRegion ? 0.62f : 0.5f);
        island(c, r, bright);
        decor(c, r, t, bright);
        if (R.playing) {   // the world being played: a ring of light round it
            float k = 0.5f + 0.5f * sinf(t * 2.5f);
            DrawCylinderWires({ c.x, c.y - 0.02f, c.z }, 5.0f + 0.2f * k, 5.0f + 0.2f * k, 0.04f, 48, Fade(COL_AMBER, 0.5f + 0.3f * k));
        }
        if (!R.open) DrawCubeWiresV({ c.x, c.y + 2.0f, c.z }, { 7.3f, 4.2f, 7.3f }, Fade(COL_MUTED, 0.35f));   // locked: a cage
    }

    // Paths round each island, and pointer bridges from each boss to the next island
    for (int i = 0; i + 1 < (int)nodes.size(); i++) {
        Vector3 a = city_node(nodes, i, rc), b = city_node(nodes, i + 1, rc);
        a.y += 0.1f; b.y += 0.1f;
        bool done = nodes[i].state == 0 && nodes[i + 1].state <= 1;
        if (nodes[i].region != nodes[i + 1].region) {
            bool lit = regions[nodes[i].region].beaten;
            bool glow = nodes[i].region == glowBridge;
            Color col = lit ? (glow ? COL_AMBER : COL_GREEN) : Color{ 120, 130, 150, 255 };
            arc(a, b, 3.0f, glow ? 0.1f : 0.07f, Fade(col, lit ? 0.95f : 0.45f), true);
            if (lit)   // a bit of data travelling along the pointer
                for (int k = 0; k < 2; k++) {
                    float u = fmodf(t * 0.35f + k * 0.5f, 1.0f);
                    DrawSphere(arc_point(a, b, 3.0f, u), glow ? 0.22f : 0.14f, glow ? COL_AMBER : Color{ 200, 255, 200, 255 });
                }
        } else {
            DrawCylinderEx(a, b, 0.07f, 0.07f, 6, Fade(done ? COL_GREEN : Color{ 140, 150, 170, 255 }, done ? 0.95f : 0.5f));
        }
    }
    for (int i = 0; i < (int)nodes.size(); i++) pad(city_node(nodes, i, rc), nodes[i], nodes[i].region, i == hoverNode, t);

    rlEnableBackfaceCulling();
    EndMode3D();
    EndTextureMode();

    BeginMode2D(fit);
    DrawTexturePro(target.texture, { 0, 0, (float)target.texture.width, -(float)target.texture.height },
                   { 0, 0, (float)SCREEN_W, (float)SCREEN_H }, { 0, 0 }, 0, WHITE);
}
