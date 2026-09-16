#include "display/screen_renderer.h"

#include "display/status_screens.h"
#include "display/usage_layouts.h"

namespace {

// The backend's age is frozen at fetch time: keep it growing between polls, and
// treat the data as stale while the backend is unreachable.
UsageView liveView(UsageView view, const AppState &state, uint32_t nowMs) {
    view.ageSeconds += (nowMs - state.usageFetchedAtMs) / 1000;
    view.stale = view.stale || state.lastFetchFailed;
    return view;
}

ProvidersUsage liveUsage(const AppState &state, uint32_t nowMs) {
    return ProvidersUsage{
        liveView(state.usage.claude, state, nowMs),
        liveView(state.usage.codex, state, nowMs),
    };
}

uint32_t pairingSecondsLeft(const AppState &state, uint32_t nowMs) {
    const uint32_t elapsed = (nowMs - state.pairingStartedAtMs) / 1000;
    const uint32_t expiresIn = state.pairCode.expiresInSeconds;
    return elapsed >= expiresIn ? 0 : expiresIn - elapsed;
}

}  // namespace

void renderScreen(U8G2 &display, const AppState &state, const FrameContext &context) {
    switch (state.screen) {
        case AppScreen::ConnectingWifi:
            drawWifiScreen(display, state.wifiStatus);
            break;
        case AppScreen::RequestingPairCode:
            drawMessageScreen(display, "Pareamento", "Gerando código...");
            break;
        case AppScreen::WaitingPairApproval:
            drawPairingScreen(display, state.pairCode, pairingSecondsLeft(state, context.nowMs));
            break;
        case AppScreen::LoadingUsage:
            drawMessageScreen(display, "Consumo", "Carregando...");
            break;
        case AppScreen::ShowingUsage:
            drawUsageScreen(display, state.layout, liveUsage(state, context.nowMs), context);
            break;
        case AppScreen::Error:
            drawErrorScreen(display, state.errorMessage);
            break;
    }
}
