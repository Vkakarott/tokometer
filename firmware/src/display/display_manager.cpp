#include "display/display_manager.h"

#include <Arduino.h>
#include <cstring>

#include "display/frame_context.h"
#include "display/indexed_canvas.h"
#include "display/jd9853_panel.h"
#include "display/status_screens.h"
#include "display/tft_renderer.h"
#include "timekeeping/ntp_clock.h"

namespace {

constexpr uint16_t PANEL_HEIGHT = IndexedCanvas::Height;
constexpr uint16_t BACKGROUND_RGB565 = TftColor::Palette[TftColor::Background];

// The busy bar and the pairing countdown move; usage text only changes by the minute.
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

IndexedCanvas canvas;
uint8_t shownRows[PANEL_HEIGHT][IndexedCanvas::RowBytes];
bool panelMatchesShownRows = false;
FrameKey lastKey;
uint32_t lastFrameAtMs = 0;

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
    fillPanel(BACKGROUND_RGB565);
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
