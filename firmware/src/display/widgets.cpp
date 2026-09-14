#include "display/widgets.h"

#include <cstdio>

#include "core/text_format.h"

namespace {

constexpr int SEGMENT_GAP = 2;
constexpr int BADGE_HEIGHT = 9;

const unsigned char WIFI_GLYPH_BITS[] = {
    0xFE, 0x00, 0x01, 0x01, 0x7C, 0x00, 0x82, 0x00, 0x38, 0x00, 0x00, 0x00, 0x10, 0x00,
};

float clampFraction(float fraction) {
    if (fraction < 0.0f) return 0.0f;
    if (fraction > 1.0f) return 1.0f;
    return fraction;
}

// Any usage above zero lights at least one segment, so 3% never looks like 0%.
int activeSegments(float fraction, int segments) {
    const float clamped = clampFraction(fraction);
    const int active = static_cast<int>(clamped * segments + 0.5f);
    return active == 0 && clamped > 0.0f ? 1 : active;
}

void formatBadgeText(uint32_t ageSeconds, char *out, size_t size) {
    char age[12];
    formatAge(ageSeconds, age, sizeof age);
    snprintf(out, size, "! %s", age);
}

}  // namespace

void drawProgressBar(U8G2 &display, int x, int y, int width, int height, float fraction) {
    const int padding = height >= 8 ? 2 : 1;
    const int innerWidth = width - 2 * padding;
    const int fillWidth = static_cast<int>(innerWidth * clampFraction(fraction) + 0.5f);

    display.drawRFrame(x, y, width, height, 1);
    if (fillWidth > 0) display.drawBox(x + padding, y + padding, fillWidth, height - 2 * padding);
}

void drawSegmentedBar(U8G2 &display, int x, int y, int width, int height, float fraction, int segments) {
    const int inner = width - 4;
    const int segmentWidth = (inner - SEGMENT_GAP * (segments - 1)) / segments;
    const int used = segmentWidth * segments + SEGMENT_GAP * (segments - 1);
    const int startX = x + 2 + (inner - used) / 2;
    const int active = activeSegments(fraction, segments);

    display.drawRFrame(x, y, width, height, 2);
    for (int i = 0; i < active; ++i) {
        display.drawBox(startX + i * (segmentWidth + SEGMENT_GAP), y + 2, segmentWidth, height - 4);
    }
}

void drawHourglassIcon(U8G2 &display, int x, int y) {
    display.drawBox(x, y, 11, 2);
    display.drawBox(x, y + 14, 11, 2);
    display.drawLine(x + 1, y + 2, x + 4, y + 7);
    display.drawLine(x + 9, y + 2, x + 6, y + 7);
    display.drawLine(x + 4, y + 8, x + 1, y + 13);
    display.drawLine(x + 6, y + 8, x + 9, y + 13);
    display.drawHLine(x + 3, y + 4, 5);
    display.drawPixel(x + 5, y + 7);
    display.drawBox(x + 3, y + 12, 5, 2);
    display.drawHLine(x + 4, y + 11, 3);
}

void drawSignalBarsIcon(U8G2 &display, int x, int y) {
    display.drawBox(x, y + 9, 3, 5);
    display.drawBox(x + 4, y + 5, 3, 9);
    display.drawBox(x + 8, y, 3, 14);
}

void drawCalendarIcon(U8G2 &display, int x, int y) {
    display.drawRFrame(x, y + 2, 13, 11, 1);
    display.drawHLine(x, y + 5, 13);
    display.drawBox(x + 3, y, 2, 4);
    display.drawBox(x + 8, y, 2, 4);
    for (int column = 0; column < 3; ++column) {
        display.drawPixel(x + 3 + column * 3, y + 8);
        display.drawPixel(x + 3 + column * 3, y + 10);
    }
}

void drawWifiGlyph(U8G2 &display, int x, int y) {
    display.drawXBM(x, y, 9, 7, WIFI_GLYPH_BITS);
}

void drawClockGlyph(U8G2 &display, int x, int y) {
    display.drawCircle(x + 3, y + 3, 3);
    display.drawVLine(x + 3, y + 1, 3);
    display.drawPixel(x + 4, y + 3);
}

void drawCenteredText(U8G2 &display, int centerX, int baselineY, const char *text) {
    display.drawUTF8(centerX - display.getUTF8Width(text) / 2, baselineY, text);
}

void drawRightText(U8G2 &display, int rightX, int baselineY, const char *text) {
    display.drawUTF8(rightX - display.getUTF8Width(text), baselineY, text);
}

void setFittingFont(U8G2 &display, const char *text, int maxWidth, std::initializer_list<const uint8_t *> fonts) {
    for (const uint8_t *font : fonts) {
        display.setFont(font);
        if (display.getUTF8Width(text) <= maxWidth) return;
    }
}

int staleBadgeWidth(U8G2 &display, uint32_t ageSeconds) {
    char text[16];
    formatBadgeText(ageSeconds, text, sizeof text);
    display.setFont(u8g2_font_5x7_tf);
    return display.getUTF8Width(text) + 4;
}

int drawStaleBadge(U8G2 &display, int x, int y, uint32_t ageSeconds) {
    char text[16];
    formatBadgeText(ageSeconds, text, sizeof text);
    const int width = staleBadgeWidth(display, ageSeconds);

    display.drawRBox(x, y, width, BADGE_HEIGHT, 1);
    display.setDrawColor(0);
    display.drawUTF8(x + 2, y + BADGE_HEIGHT - 1, text);
    display.setDrawColor(1);
    return width;
}
