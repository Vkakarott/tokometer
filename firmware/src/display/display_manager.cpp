#include "display/display_manager.h"

#include <AnimatedGIF.h>
#include <Arduino.h>
#include <cstring>

#include "display/frame_context.h"
#include "display/indexed_canvas.h"
#include "display/jd9853_panel.h"
#include "display/status_screens.h"
#include "display/tft_renderer.h"
#include "timekeeping/ntp_clock.h"

namespace {

constexpr uint16_t PANEL_WIDTH = IndexedCanvas::Width;
constexpr uint16_t PANEL_HEIGHT = IndexedCanvas::Height;
constexpr uint16_t BACKGROUND_RGB565 = TftColor::Palette[TftColor::Background];

// The pairing countdown ticks every second; usage text only moves by the minute.
constexpr uint32_t STATUS_REFRESH_MS = 500;
constexpr uint32_t USAGE_REFRESH_MS = 5000;

// Everything that must reach the screen at once instead of on the next refresh.
struct FrameKey {
    AppScreen screen;
    WifiStatus wifiStatus;
    UsageLayout layout;
    bool sevenDay;
    bool lastFetchFailed;
    uint32_t usageFetchedAtMs;
    int64_t clockMinute;

    bool operator==(const FrameKey &other) const {
        return screen == other.screen && wifiStatus == other.wifiStatus && layout == other.layout &&
               sevenDay == other.sevenDay && lastFetchFailed == other.lastFetchFailed &&
               usageFetchedAtMs == other.usageFetchedAtMs && clockMinute == other.clockMinute;
    }
};

AnimatedGIF bootAnimation;
IndexedCanvas canvas;
uint8_t shownRows[PANEL_HEIGHT][IndexedCanvas::RowBytes];
bool panelMatchesShownRows = false;
FrameKey lastKey;
uint32_t lastFrameAtMs = 0;

extern const uint8_t bootAnimationStart[] asm("_binary_firmware_assets_agents_pixel_boot_296x240_gif_start");
extern const uint8_t bootAnimationEnd[] asm("_binary_firmware_assets_agents_pixel_boot_296x240_gif_end");

void clearPanel() {
    fillPanel(BACKGROUND_RGB565);
    panelMatchesShownRows = false;
}

// Under disposal method 2 a transparent pixel means "restore the background".
void restoreBackground(GIFDRAW *draw, int first, int last) {
    for (int index = first; index < last; ++index) {
        if (draw->pPixels[index] == draw->ucTransparent) draw->pPixels[index] = draw->ucBackground;
    }
    draw->ucHasTransparency = 0;
}

// Transparent pixels keep what the previous frame drew, so only opaque runs are sent.
void drawBootAnimationLine(GIFDRAW *draw) {
    const int y = draw->iY + draw->y;
    if (y < 0 || y >= PANEL_HEIGHT) return;

    const int first = max(0, -draw->iX);
    const int last = min(draw->iWidth, PANEL_WIDTH - draw->iX);
    if (draw->ucDisposalMethod == 2) restoreBackground(draw, first, last);

    const bool transparency = draw->ucHasTransparency;
    int start = first;
    while (start < last) {
        while (start < last && transparency && draw->pPixels[start] == draw->ucTransparent) ++start;
        int end = start;
        while (end < last && !(transparency && draw->pPixels[end] == draw->ucTransparent)) ++end;
        if (start < end) {
            writePanelPixels(draw->iX + start, y, draw->pPixels + start, draw->pPalette, end - start);
        }
        start = end;
    }
}

FrameKey frameKeyFor(const AppState &state, int64_t nowSeconds) {
    return {state.screen, state.wifiStatus, state.layout, state.sevenDay,
            state.lastFetchFailed, state.usageFetchedAtMs, nowSeconds / 60};
}

bool isFrameDue(const AppState &state, const FrameKey &key, uint32_t nowMs) {
    if (!panelMatchesShownRows || !(key == lastKey)) return true;
    const uint32_t refreshMs = state.screen == AppScreen::ShowingUsage ? USAGE_REFRESH_MS : STATUS_REFRESH_MS;
    return nowMs - lastFrameAtMs >= refreshMs;
}

UsageView liveUsageView(UsageView view, const AppState &state, uint32_t nowMs) {
    view.ageSeconds += (nowMs - state.usageFetchedAtMs) / 1000;
    view.stale = view.stale || state.lastFetchFailed;
    return view;
}

void renderUsageScreen(const AppState &state, const FrameContext &context) {
    const ProvidersUsage usage{
        liveUsageView(state.usage.claude, state, context.nowMs),
        liveUsageView(state.usage.codex, state, context.nowMs),
        liveUsageView(state.usage.cursor, state, context.nowMs),
    };
    const uint8_t provider = state.layout == UsageLayout::ClaudeDashboard ? 1 :
                             state.layout == UsageLayout::CursorDashboard ? 2 : 0;
    renderTftUsage(canvas, usage, provider, state.layout == UsageLayout::Overview,
                   state.sevenDay, context);
}

bool rowChanged(uint16_t y) {
    return memcmp(shownRows[y], canvas.row(y), IndexedCanvas::RowBytes) != 0;
}

// Sends the span between the first and the last changed row: a clock tick costs
// a few rows instead of the whole 142 KB frame.
void sendChangedRows() {
    uint16_t first = 0;
    uint16_t last = PANEL_HEIGHT;
    if (panelMatchesShownRows) {
        while (first < PANEL_HEIGHT && !rowChanged(first)) ++first;
        if (first == PANEL_HEIGHT) return;
        while (!rowChanged(last - 1)) --last;
    }
    sendCanvasRows(canvas, first, last - first);
    memcpy(shownRows[first], canvas.row(first), static_cast<size_t>(last - first) * IndexedCanvas::RowBytes);
    panelMatchesShownRows = true;
}

}  // namespace

void initializeDisplay() {
    initializePanel();
    clearPanel();
}

void playBootAnimation() {
    bootAnimation.begin(GIF_PALETTE_RGB565_LE);
    const size_t size = bootAnimationEnd - bootAnimationStart;
    if (!bootAnimation.openFLASH(const_cast<uint8_t *>(bootAnimationStart), size, drawBootAnimationLine)) {
        log_e("Could not open the boot animation");
        return;
    }
    while (bootAnimation.playFrame(true, nullptr)) delay(1);
    bootAnimation.close();
    clearPanel();
}

void drawFrame(const AppState &state, uint32_t nowMs) {
    FrameContext context;
    context.nowMs = nowMs;
    context.nowSeconds = currentEpochSeconds();
    const FrameKey key = frameKeyFor(state, context.nowSeconds);
    if (!isFrameDue(state, key, nowMs)) return;

    formatClockTime(context.clockText, sizeof context.clockText);
    if (state.screen == AppScreen::ShowingUsage) {
        renderUsageScreen(state, context);
    } else {
        renderStatusScreen(canvas, state, context);
    }
    sendChangedRows();
    lastKey = key;
    lastFrameAtMs = nowMs;
}
