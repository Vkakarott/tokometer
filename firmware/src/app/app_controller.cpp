#include "app/app_controller.h"

#include <Arduino.h>
#include <cstring>

#include "config/api_config.h"
#include "input/button.h"
#include "network/api_client.h"
#include "storage/device_store.h"

namespace {

constexpr char BACKEND_UNREACHABLE[] = "Backend inacessível";

bool isActionDue(const AppState &state, uint32_t nowMs) {
    return nowMs - state.lastActionAtMs >= state.actionDelayMs;
}

void scheduleAction(AppState &state, uint32_t nowMs, uint32_t delayMs) {
    state.lastActionAtMs = nowMs;
    state.actionDelayMs = delayMs;
}

void showError(AppState &state, const char *message, uint32_t nowMs) {
    log_w("%s", message);
    state.errorMessage = message;
    state.screen = AppScreen::Error;
    scheduleAction(state, nowMs, ApiConfig::RETRY_DELAY_MS);
}

void handleButton(AppState &state, uint32_t nowMs) {
    if (!pollButtonPress(nowMs) || state.screen != AppScreen::ShowingUsage) return;

    const uint8_t next = (static_cast<uint8_t>(state.layout) + 1) % USAGE_LAYOUT_COUNT;
    state.layout = static_cast<UsageLayout>(next);
    saveLayoutIndex(next);
}

// Screens that already hold data come straight back; the others are re-entered
// by the pairing/usage flow on its next due action.
void resumeAfterReconnect(AppState &state) {
    if (state.screen != AppScreen::ConnectingWifi) return;
    if (state.hasToken && state.hasUsage) {
        state.screen = AppScreen::ShowingUsage;
    } else if (!state.hasToken && state.pairingActive) {
        state.screen = AppScreen::WaitingPairApproval;
    }
}

void startPairing(AppState &state, uint32_t nowMs) {
    if (state.screen != AppScreen::RequestingPairCode) {
        state.screen = AppScreen::RequestingPairCode;  // render before the blocking request
        return;
    }
    if (!requestPairCode(state.pairCode)) {
        showError(state, BACKEND_UNREACHABLE, nowMs);
        return;
    }
    log_i("pairing code %s", state.pairCode.userCode);
    state.pairingActive = true;
    state.pairingStartedAtMs = nowMs;
    state.screen = AppScreen::WaitingPairApproval;
    scheduleAction(state, nowMs, state.pairCode.intervalSeconds * 1000UL);
}

void completePairing(AppState &state, const TokenPoll &poll) {
    log_i("device paired");
    saveAccessToken(poll.accessToken);
    memcpy(state.accessToken, poll.accessToken, sizeof state.accessToken);
    state.hasToken = true;
    state.pairingActive = false;
    state.screen = AppScreen::LoadingUsage;
    state.actionDelayMs = 0;
}

void pollPairing(AppState &state, uint32_t nowMs) {
    const TokenPoll poll = pollPairToken(state.pairCode.deviceCode);
    scheduleAction(state, nowMs, state.pairCode.intervalSeconds * 1000UL);

    switch (poll.outcome) {
        case TokenPollOutcome::Approved:
            completePairing(state, poll);
            break;
        case TokenPollOutcome::Expired:
        case TokenPollOutcome::Denied:
            log_i("pairing code no longer valid, requesting a new one");
            state.pairingActive = false;
            state.actionDelayMs = 0;
            break;
        case TokenPollOutcome::Pending:
        case TokenPollOutcome::Failed:
            break;
    }
}

void updatePairing(AppState &state, uint32_t nowMs) {
    if (!isActionDue(state, nowMs)) return;
    if (state.pairingActive) {
        pollPairing(state, nowMs);
    } else {
        startPairing(state, nowMs);
    }
}

void forgetToken(AppState &state) {
    log_w("token rejected by backend, pairing again");
    clearAccessToken();
    state.hasToken = false;
    state.accessToken[0] = '\0';
    state.hasUsage = false;
    state.actionDelayMs = 0;
}

void storeUsage(AppState &state, const ProvidersUsage &view, uint32_t nowMs) {
    state.usage = view;
    state.hasUsage = true;
    state.lastFetchFailed = false;
    state.usageFetchedAtMs = nowMs;
    state.screen = AppScreen::ShowingUsage;
}

// With data already on screen a failed poll only marks it as stale; without
// data there is nothing honest to show but the error.
void handleUsageFailure(AppState &state, uint32_t nowMs) {
    state.lastFetchFailed = true;
    if (!state.hasUsage) showError(state, BACKEND_UNREACHABLE, nowMs);
}

void updateUsage(AppState &state, uint32_t nowMs) {
    if (!isActionDue(state, nowMs)) return;
    if (!state.hasUsage && state.screen != AppScreen::LoadingUsage) {
        state.screen = AppScreen::LoadingUsage;  // render before the blocking request
        return;
    }

    ProvidersUsage view;
    const UsageFetchStatus status = fetchUsage(state.accessToken, view);
    scheduleAction(state, nowMs, ApiConfig::USAGE_POLL_INTERVAL_MS);

    if (status == UsageFetchStatus::Ok) {
        storeUsage(state, view, nowMs);
    } else if (status == UsageFetchStatus::Unauthorized) {
        forgetToken(state);
    } else {
        handleUsageFailure(state, nowMs);
    }
}

}  // namespace

void initializeApp(AppState &state) {
    state.hasToken = loadAccessToken(state.accessToken, sizeof state.accessToken);
    const uint8_t layout = loadLayoutIndex();
    state.layout = static_cast<UsageLayout>(layout < USAGE_LAYOUT_COUNT ? layout : 0);
    log_i("boot: %s", state.hasToken ? "token loaded" : "no token, pairing required");
}

void updateApp(AppState &state, uint32_t nowMs) {
    handleButton(state, nowMs);

    state.wifiStatus = updateWifi();
    if (state.wifiStatus != WifiStatus::Connected) {
        state.screen = AppScreen::ConnectingWifi;
        return;
    }

    resumeAfterReconnect(state);
    if (state.hasToken) {
        updateUsage(state, nowMs);
    } else {
        updatePairing(state, nowMs);
    }
}
