#pragma once
#include <cstdint>
struct GaugePoint { int16_t x; int16_t y; };
float rpmToAngleDegrees(uint16_t rpm);
GaugePoint pointOnArc(int16_t cx, int16_t cy, int16_t radius, float degrees);
