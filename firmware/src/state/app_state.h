#pragma once

#include <cstdint>

#include "core/api_codec.h"
#include "core/usage_types.h"
#include "network/wifi_manager.h"

enum class AppScreen {
    ConnectingWifi,
    RequestingPairCode,
    WaitingPairApproval,
    LoadingUsage,
    ShowingUsage,
    Error
};

enum class UsageLayout : uint8_t {
    Diagonal,
    Cards,
    Focus
};

constexpr uint8_t USAGE_LAYOUT_COUNT = 3;

struct AppState {
    WifiStatus wifiStatus = WifiStatus::Disconnected;
    AppScreen screen = AppScreen::ConnectingWifi;
    UsageLayout layout = UsageLayout::Diagonal;

    bool hasToken = false;
    char accessToken[sizeof(TokenPoll::accessToken)] = "";

    bool pairingActive = false;
    PairCode pairCode;
    uint32_t pairingStartedAtMs = 0;

    bool hasUsage = false;
    bool lastFetchFailed = false;
    UsageView usage;
    uint32_t usageFetchedAtMs = 0;

    const char *errorMessage = "";

    // Next network action runs once actionDelayMs has elapsed since lastActionAtMs.
    uint32_t lastActionAtMs = 0;
    uint32_t actionDelayMs = 0;
};
