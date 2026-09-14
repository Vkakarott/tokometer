#include "display/usage_layouts.h"

#include "core/text_format.h"
#include "display/widgets.h"

namespace {

constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr int GLYPH_TEXT_GAP = 9;

using IconDrawer = void (*)(U8G2 &, int, int);

struct WindowText {
    char value[12];
    char reset[12];
};

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

// ---------------------------------------------------------------------------
// Layout 1: diagonal split, 5H top-left and 7D bottom-right.
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
    drawResetLabel(display, SCREEN_WIDTH - 4 - resetLabelWidth(display, text.reset), 12, text.reset);
}

void drawDiagonalLayout(U8G2 &display, const UsageView &usage, const WindowText &five, const WindowText &seven) {
    display.drawRFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 3);
    display.drawLine(90, 2, 78, 23);
    display.drawHLine(70, 23, 9);
    display.drawLine(70, 23, 50, 61);

    drawDiagonalFiveHour(display, usage.fiveHour, five);
    drawDiagonalSevenDay(display, usage.sevenDay, seven);
    if (usage.stale) drawStaleBadge(display, 4, 53, usage.ageSeconds);
}

// ---------------------------------------------------------------------------
// Layout 2: header plus two side-by-side cards.
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
// Layout 3: 5H in focus on top, 7D summary line at the bottom.
// ---------------------------------------------------------------------------

void drawFocusTop(U8G2 &display, const UsageView &usage, const WindowText &five, const FrameContext &context) {
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(1, 7, "5H USO");
    drawRightText(display, SCREEN_WIDTH - 1, 7, context.clockText);
    drawWifiGlyph(display, context.clockText[0] == '\0' ? SCREEN_WIDTH - 10 : 91, 0);

    setFittingFont(display, five.value, 70, {u8g2_font_fub20_tf, u8g2_font_fub17_tf, u8g2_font_fub14_tf});
    display.drawUTF8(0, 36, five.value);

    drawResetLabel(display, SCREEN_WIDTH - 1 - resetLabelWidth(display, five.reset), 20, five.reset);
    drawProgressBar(display, 72, 24, 56, 7, usageFillFraction(usage.fiveHour));
    if (usage.stale) {
        drawStaleBadge(display, SCREEN_WIDTH - staleBadgeWidth(display, usage.ageSeconds), 33, usage.ageSeconds);
    }
    display.drawHLine(0, 44, SCREEN_WIDTH);
}

void drawFocusBottom(U8G2 &display, const UsageWindow &window, const WindowText &seven) {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(1, 59, "7D");
    display.setFont(u8g2_font_7x13B_tf);
    display.drawStr(16, 60, seven.value);
    drawProgressBar(display, 50, 50, 44, 9, usageFillFraction(window));
    display.setFont(u8g2_font_5x7_tf);
    drawRightText(display, SCREEN_WIDTH - 1, 58, seven.reset);
}

void drawNoDataScreen(U8G2 &display) {
    display.setFont(u8g2_font_6x10_tf);
    drawCenteredText(display, SCREEN_WIDTH / 2, 24, "Sem dados");
    display.drawHLine(SCREEN_WIDTH / 2 - 30, 29, 60);
    display.setFont(u8g2_font_5x7_tf);
    drawCenteredText(display, SCREEN_WIDTH / 2, 42, "Abra o Claude Code e");
    drawCenteredText(display, SCREEN_WIDTH / 2, 52, "envie uma mensagem");
}

}  // namespace

void drawUsageScreen(U8G2 &display, UsageLayout layout, const UsageView &usage, const FrameContext &context) {
    if (!usage.hasData) {
        drawNoDataScreen(display);
        return;
    }

    const WindowText five = describeWindow(usage.fiveHour, context.nowSeconds);
    const WindowText seven = describeWindow(usage.sevenDay, context.nowSeconds);
    switch (layout) {
        case UsageLayout::Diagonal:
            drawDiagonalLayout(display, usage, five, seven);
            break;
        case UsageLayout::Cards:
            drawCardsLayout(display, usage, five, seven, context);
            break;
        case UsageLayout::Focus:
            drawFocusTop(display, usage, five, context);
            drawFocusBottom(display, usage.sevenDay, seven);
            break;
    }
}
