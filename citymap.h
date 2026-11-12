// citymap.h - the parts of Memory City (memcity.cpp) that don't draw: where
// each island and stop sits, and what each island's label says. Kept apart
// from the drawing so tests/test_core.cpp can test them without raylib.

#pragma once
#include <string>
#include <vector>
#include "campaign.h"

struct CityPoint { float x = 0, y = 0, z = 0; };

struct CityRegion {
    std::string name;        // "The Heap"
    std::string status;      // "2 / 3 topics", "LOCKED", "BOSS READY", "CLEARED"
    bool open = false, beaten = false, playing = false;
};

struct CityNode {
    int region = 0;
    int topic = 0;           // -1 = the region's boss
    int state = 0;           // 0 done, 1 current, 2 to do, 3 locked (the same order as the 2D map's NodeState)
};

const float CITY_ISLAND_SIZE = 7.0f;   // each island is a 7 x 7 slab

// Where an island sits (y is the top of the island), and the city's middle.
CityPoint city_island_at(int region, int regionCount);
CityPoint city_centre_at(int regionCount);

// Where stop `index` sits. Each island's stops go round its edge the long way,
// from the side facing the island before to the side facing the island after.
CityPoint city_node_at(const std::vector<CityNode> &nodes, int index, int regionCount);

// Each region's label and state, worked out from the campaign and progress.
std::vector<CityRegion> city_regions_for(const std::vector<Region> &campaign, const Progress &p);
