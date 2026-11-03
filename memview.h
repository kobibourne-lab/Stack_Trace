// memview.h - draws one lesson step's memory: stack frames on the left (the
// newest frame on top), heap blocks on the right, and an arrow from every
// pointer to whatever it points at. Heap blocks nothing points at are marked
// as leaked.

#pragma once
#include "raylib.h"
#include "content.h"

void memview_draw(const Step &step, Rectangle area);
