// memcity.h - Memory City: the campaign seen from above in 3D. Each region of
// memory is a floating island (the Stack's tower of frames, Address Space's
// field of cells, the Heap's scattered blocks, Object Land's buildings),
// its topics are glowing pads around the edge, and pointer arcs bridge one
// island to the next. You can turn it and zoom it; clicking an island dives
// into that world's 2D map (screen_campaign.cpp does the diving).
//
// This file only draws. Where things sit and what the labels say is in
// citymap.cpp (tested); what's cleared or open comes from campaign.cpp.

#pragma once
#include "raylib.h"
#include <string>
#include <vector>
#include "citymap.h"   // CityRegion, CityNode and where things sit

// The camera: which way it faces, how far it's zoomed, what it looks at.
struct CityCam {
    float yaw = 0.785f;      // radians around the city; 0.785 = the classic diagonal view
    float fovy = 26;         // orthographic height in world units: smaller = closer
    Vector3 target = { 0, 0, 0 };
};

// Where an island and a pad sit in the world (y is the top of the island).
Vector3 city_island(int region, int regionCount);
Vector3 city_node(const std::vector<CityNode> &nodes, int index, int regionCount);

// The whole city's middle, for the camera when it's zoomed out.
Vector3 city_centre(int regionCount);

// Draw the city into the current screen area (SCREEN_W x SCREEN_H).
// hoverRegion/hoverNode light up what the mouse is over (-1 = nothing);
// glowBridge lights the bridge leaving that region brighter (a boss just fell).
void city_draw(const std::vector<CityRegion> &regions, const std::vector<CityNode> &nodes, const CityCam &cam,
               float t, int hoverRegion, int hoverNode, int glowBridge);

// A point in the world, on the screen (in the app's 1280 x 720 coordinates).
Vector2 city_to_screen(Vector3 p, const CityCam &cam);

// Free the off-screen image the city is drawn into (at shutdown).
void city_unload();
