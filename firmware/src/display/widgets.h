#pragma once

#include <U8g2lib.h>
#include <initializer_list>

void drawProgressBar(U8G2 &display, int x, int y, int width, int height, float fraction);
void drawSegmentedBar(U8G2 &display, int x, int y, int width, int height, float fraction, int segments);

void drawHourglassIcon(U8G2 &display, int x, int y);   // 11x16
void drawSignalBarsIcon(U8G2 &display, int x, int y);  // 11x14
void drawCalendarIcon(U8G2 &display, int x, int y);    // 13x13
void drawWifiGlyph(U8G2 &display, int x, int y);       // 9x7
void drawClockGlyph(U8G2 &display, int x, int y);      // 7x7

// Text helpers use the current font and UTF-8, so pt-BR accents render.
void drawCenteredText(U8G2 &display, int centerX, int baselineY, const char *text);
void drawRightText(U8G2 &display, int rightX, int baselineY, const char *text);

// Sets the first font in which `text` fits within maxWidth, or the last one.
void setFittingFont(U8G2 &display, const char *text, int maxWidth, std::initializer_list<const uint8_t *> fonts);

// Inverted "! <age>" badge with its top-left corner at (x, y). Returns its width.
int drawStaleBadge(U8G2 &display, int x, int y, uint32_t ageSeconds);
int staleBadgeWidth(U8G2 &display, uint32_t ageSeconds);
