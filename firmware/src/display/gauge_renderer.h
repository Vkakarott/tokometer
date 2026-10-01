#pragma once
#include <cstdint>
#include "display/indexed_canvas.h"
constexpr uint16_t GaugeRegionX=20, GaugeRegionY=8, GaugeRegionWidth=256, GaugeRegionHeight=224;
void drawGauge(IndexedCanvas &canvas,uint16_t needleRpm,uint16_t readoutRpm,bool noData);
