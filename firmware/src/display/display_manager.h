#pragma once

#include <cstdint>

#include "display/indexed_canvas.h"

void initializeDisplay();
void showBootLogo();
void showStatus(const char *title, const char *subtitle);
IndexedCanvas &gaugeCanvas();
void pushRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
