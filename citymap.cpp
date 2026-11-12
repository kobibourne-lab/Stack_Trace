// citymap.cpp - Memory City's layout and labels (see citymap.h).

#include "citymap.h"
#include <cmath>

const float CITY_PI = 3.14159265f;

CityPoint city_island_at(int region, int regionCount)
{
    static const float lift[4] = { 0.0f, 0.6f, 0.2f, 0.9f };   // islands float at slightly different heights
    CityPoint p;
    p.x = (region - (regionCount - 1) / 2.0f) * 7.4f;
    p.y = lift[region % 4];
    p.z = region % 2 == 0 ? 3.6f : -3.6f;                       // zigzag front and back
    return p;
}

CityPoint city_centre_at(int regionCount)
{
    (void)regionCount;
    return { 0, 0.3f, 0 };
}

CityPoint city_node_at(const std::vector<CityNode> &nodes, int index, int regionCount)
{
    const CityNode &n = nodes[index];
    int r = n.region, count = 0, pos = 0;
    for (int i = 0; i < (int)nodes.size(); i++)
        if (nodes[i].region == r) { if (i == index) pos = count; count++; }
    CityPoint c = city_island_at(r, regionCount);
    CityPoint prev = r > 0 ? city_island_at(r - 1, regionCount) : CityPoint{ c.x - 7, c.y, c.z };
    CityPoint next = r + 1 < regionCount ? city_island_at(r + 1, regionCount) : CityPoint{ c.x + 7, c.y, c.z };
    float a0 = atan2f(prev.z - c.z, prev.x - c.x), a1 = atan2f(next.z - c.z, next.x - c.x);
    float d = a1 - a0;
    while (d < 0) d += 2 * CITY_PI;
    while (d >= 2 * CITY_PI) d -= 2 * CITY_PI;
    if (d < CITY_PI) d -= 2 * CITY_PI;   // always the long way round
    float u = count > 1 ? 0.08f + 0.84f * pos / (count - 1) : 0.5f;
    float a = a0 + d * u;
    float rad = n.topic < 0 ? 2.25f : 2.55f;
    return { c.x + cosf(a) * rad, c.y + 0.02f, c.z + sinf(a) * rad };
}

std::vector<CityRegion> city_regions_for(const std::vector<Region> &campaign, const Progress &p)
{
    std::vector<CityRegion> out;
    int playing = current_region(campaign, p);
    for (int r = 0; r < (int)campaign.size(); r++) {
        const Region &reg = campaign[r];
        CityRegion c;
        c.name = reg.name;
        c.open = region_open(campaign, p, r);
        c.beaten = region_beaten(campaign, p, r);
        c.playing = c.open && r == playing;
        int done = (int)reg.topics.size() - region_topics_left(reg, p);
        if (c.beaten) c.status = "CLEARED";
        else if (!c.open) c.status = "LOCKED";
        else if (done == (int)reg.topics.size()) c.status = "BOSS READY";
        else c.status = std::to_string(done) + " / " + std::to_string(reg.topics.size()) + " topics";
        out.push_back(c);
    }
    return out;
}
