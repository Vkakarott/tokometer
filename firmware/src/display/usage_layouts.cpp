#include "display/usage_layouts.h"

#include <cstdio>

#include "core/text_format.h"
#include "display/widgets.h"

namespace {

constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr int GLYPH_TEXT_GAP = 9;
constexpr int TAG_HEIGHT = 9;

using IconDrawer = void (*)(U8G2 &, int, int);

struct WindowText {
    char value[12];
    char reset[12];
};

using ProviderLayout = void (*)(U8G2 &, const UsageView &, const WindowText &, const WindowText &,
                                const FrameContext &);

WindowText describeWindow(const UsageWindow &window, int64_t nowSeconds) {
    WindowText text;
    formatPrimaryValue(window, text.value, sizeof text.value);
    formatResetIn(window, nowSeconds, text.reset, sizeof text.reset);
    return text;
}

// Clock glyph followed by the reset countdown, left edge at x. Nothing when the
// countdown is unknown.
void drawResetLabel(U8G2 &display, int x, int baselineY, const char *reset) {
    if (reset[0] == '\0') return;
    drawClockGlyph(display, x, baselineY - 6);
    display.setFont(u8g2_font_5x7_tf);
    display.drawUTF8(x + GLYPH_TEXT_GAP, baselineY, reset);
}

int resetLabelWidth(U8G2 &display, const char *reset) {
    if (reset[0] == '\0') return 0;
    display.setFont(u8g2_font_5x7_tf);
    return GLYPH_TEXT_GAP + display.getUTF8Width(reset);
}

// Inverted provider name, right edge at rightX.
void drawProviderTag(U8G2 &display, int rightX, int y, const char *name) {
    display.setFont(u8g2_font_5x7_tf);
    const int width = display.getUTF8Width(name) + 4;
    const int x = rightX - width;
    display.drawRBox(x, y, width, TAG_HEIGHT, 1);
    display.setDrawColor(0);
    display.drawUTF8(x + 2, y + TAG_HEIGHT - 1, name);
    display.setDrawColor(1);
}

// ---------------------------------------------------------------------------
// Codex: diagonal split, 5H top-left and 7D bottom-right.
// ---------------------------------------------------------------------------

void drawDiagonalFiveHour(U8G2 &display, const UsageWindow &window, const WindowText &text) {
    drawHourglassIcon(display, 5, 5);
    display.drawVLine(19, 5, 22);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(23, 11, "5H");
    setFittingFont(display, text.value, 41, {u8g2_font_fub14_tf, u8g2_font_fub11_tf});
    display.drawUTF8(23, 28, text.value);
    drawSegmentedBar(display, 4, 31, 54, 9, usageFillFraction(window), 8);
    drawResetLabel(display, 5, 50, text.reset);
}

void drawDiagonalSevenDay(U8G2 &display, const UsageWindow &window, const WindowText &text) {
    drawSignalBarsIcon(display, 69, 31);
    display.drawVLine(83, 30, 21);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(87, 35, "7D");
    setFittingFont(display, text.value, 37, {u8g2_font_fub14_tf, u8g2_font_fub11_tf, u8g2_font_7x13B_tf});
    display.drawUTF8(87, 51, text.value);
    drawSegmentedBar(display, 56, 53, 68, 9, usageFillFraction(window), 8);
    drawResetLabel(display, SCREEN_WIDTH - 4 - resetLabelWidth(display, text.reset), 21, text.reset);
}

void drawDiagonalLayout(U8G2 &display, const UsageView &usage, const WindowText &five, const WindowText &seven,
                        const FrameContext &) {
    display.drawRFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 3);
    display.drawLine(90, 2, 78, 23);
    display.drawHLine(70, 23, 9);
    display.drawLine(70, 23, 50, 61);

    drawProviderTag(display, SCREEN_WIDTH - 4, 3, "CODEX");
    drawDiagonalFiveHour(display, usage.fiveHour, five);
    drawDiagonalSevenDay(display, usage.sevenDay, seven);
    if (usage.stale) drawStaleBadge(display, 4, 53, usage.ageSeconds);
}

// ---------------------------------------------------------------------------
// Claude: header plus two side-by-side cards.
// ---------------------------------------------------------------------------

void drawCardsHeader(U8G2 &display, const UsageView &usage, const FrameContext &context) {
    drawWifiGlyph(display, 1, 1);
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(14, 8, "CLAUDE");
    drawRightText(display, SCREEN_WIDTH - 1, 8, context.clockText);
    if (usage.stale) {
        const int clockLeft = SCREEN_WIDTH - 1 - display.getUTF8Width(context.clockText);
        const int badgeWidth = staleBadgeWidth(display, usage.ageSeconds);
        drawStaleBadge(display, clockLeft - 4 - badgeWidth, 1, usage.ageSeconds);
    }
    display.drawHLine(0, 11, SCREEN_WIDTH);
}

void drawCard(U8G2 &display, int x, const char *label, IconDrawer icon, const UsageWindow &window,
              const WindowText &text) {
    display.drawRFrame(x, 14, 62, 50, 3);
    icon(display, x + 4, 18);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(x + 21, 25, label);
    drawResetLabel(display, x + 21, 34, text.reset);
    setFittingFont(display, text.value, 56, {u8g2_font_fub14_tf, u8g2_font_fub11_tf});
    drawCenteredText(display, x + 31, 52, text.value);
    drawProgressBar(display, x + 4, 55, 54, 6, usageFillFraction(window));
}

void drawCardsLayout(U8G2 &display, const UsageView &usage, const WindowText &five, const WindowText &seven,
                     const FrameContext &context) {
    drawCardsHeader(display, usage, context);
    drawCard(display, 0, "5H", drawSignalBarsIcon, usage.fiveHour, five);
    drawCard(display, 66, "7D", drawCalendarIcon, usage.sevenDay, seven);
}

// ---------------------------------------------------------------------------
// Comparison: a row per provider, 5H and 7D columns.
// ---------------------------------------------------------------------------

constexpr int NAME_COLUMN_X = 1;
constexpr int FIVE_HOUR_COLUMN_X = 38;
constexpr int SEVEN_DAY_COLUMN_X = 84;
constexpr int COLUMN_WIDTH = 42;
constexpr int COMPARISON_HEADER_BOTTOM = 9;
constexpr int COMPARISON_ROW_HEIGHT = 27;

void drawComparisonHeader(U8G2 &display, const FrameContext &context) {
    display.setFont(u8g2_font_5x7_tf);
    display.drawUTF8(NAME_COLUMN_X, 7, context.clockText);
    drawCenteredText(display, FIVE_HOUR_COLUMN_X + COLUMN_WIDTH / 2, 7, "5H");
    drawCenteredText(display, SEVEN_DAY_COLUMN_X + COLUMN_WIDTH / 2, 7, "7D");
    display.drawHLine(0, COMPARISON_HEADER_BOTTOM, SCREEN_WIDTH);
}

void drawComparisonCell(U8G2 &display, int x, int top, const UsageWindow &window) {
    char value[12];
    formatPrimaryValue(window, value, sizeof value);
    display.setFont(u8g2_font_7x13B_tf);
    display.drawUTF8(x, top + 12, value);
    drawProgressBar(display, x, top + 16, COLUMN_WIDTH, 7, usageFillFraction(window));
}

void drawStaleNote(U8G2 &display, int top, uint32_t ageSeconds) {
    char age[12];
    char note[16];
    formatAge(ageSeconds, age, sizeof age);
    snprintf(note, sizeof note, "! %s", age);
    display.setFont(u8g2_font_4x6_tf);
    display.drawUTF8(NAME_COLUMN_X, top + 21, note);
}

void drawComparisonRow(U8G2 &display, int top, const char *name, const UsageView &usage) {
    display.setFont(u8g2_font_5x7_tf);
    display.drawUTF8(NAME_COLUMN_X, top + 10, name);
    if (!usage.hasData) {
        drawCenteredText(display, (FIVE_HOUR_COLUMN_X + SCREEN_WIDTH) / 2, top + 16, "sem dados");
        return;
    }
    if (usage.stale) drawStaleNote(display, top, usage.ageSeconds);
    drawComparisonCell(display, FIVE_HOUR_COLUMN_X, top, usage.fiveHour);
    drawComparisonCell(display, SEVEN_DAY_COLUMN_X, top, usage.sevenDay);
}

void drawComparisonLayout(U8G2 &display, const ProvidersUsage &usage, const FrameContext &context) {
    const int firstRow = COMPARISON_HEADER_BOTTOM + 2;
    const int secondRow = firstRow + COMPARISON_ROW_HEIGHT;
    drawComparisonHeader(display, context);
    drawComparisonRow(display, firstRow, "CLAUDE", usage.claude);
    display.drawHLine(0, secondRow - 1, SCREEN_WIDTH);
    drawComparisonRow(display, secondRow, "CODEX", usage.codex);
}

// ---------------------------------------------------------------------------
// Single-provider screens.
// ---------------------------------------------------------------------------

void drawNoDataScreen(U8G2 &display, const char *providerName) {
    char line[32];
    snprintf(line, sizeof line, "do %s", providerName);
    display.setFont(u8g2_font_6x10_tf);
    drawCenteredText(display, SCREEN_WIDTH / 2, 24, "Sem dados");
    display.drawHLine(SCREEN_WIDTH / 2 - 30, 29, 60);
    display.setFont(u8g2_font_5x7_tf);
    drawCenteredText(display, SCREEN_WIDTH / 2, 42, "Confira o collector");
    drawCenteredText(display, SCREEN_WIDTH / 2, 52, line);
}

void drawProviderScreen(U8G2 &display, const char *providerName, const UsageView &usage,
                        const FrameContext &context, ProviderLayout layout) {
    if (!usage.hasData) {
        drawNoDataScreen(display, providerName);
        return;
    }
    const WindowText five = describeWindow(usage.fiveHour, context.nowSeconds);
    const WindowText seven = describeWindow(usage.sevenDay, context.nowSeconds);
    layout(display, usage, five, seven, context);
}

}  // namespace

void drawUsageScreen(U8G2 &display, UsageLayout layout, const ProvidersUsage &usage, const FrameContext &context) {
    switch (layout) {
        case UsageLayout::CodexDiagonal:
            drawProviderScreen(display, "Codex", usage.codex, context, drawDiagonalLayout);
            break;
        case UsageLayout::ClaudeCards:
            drawProviderScreen(display, "Claude", usage.claude, context, drawCardsLayout);
            break;
        case UsageLayout::Comparison:
            drawComparisonLayout(display, usage, context);
            break;
    }
}
