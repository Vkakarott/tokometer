#pragma once

#include <cstdint>

#include "display/indexed_canvas.h"

// SPI driver for the JD9853 panel in 296x240 landscape, RGB565.
void initializePanel();
void fillPanel(uint16_t color);

// Writes one horizontal run of palette-indexed pixels.
void writePanelPixels(uint16_t x, uint16_t y, const uint8_t *pixels, const uint16_t *palette,
                      uint16_t length);

// Sends full-width canvas rows [firstRow, firstRow + rowCount).
void sendCanvasRows(const IndexedCanvas &canvas, uint16_t firstRow, uint16_t rowCount);
