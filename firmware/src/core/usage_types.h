#pragma once

#include <cstdint>

// One subscription window as the device sees it. `present` is false when the
// backend did not send the window at all; `known` is false when it sent a null
// percentage because the window already reset.
struct UsageWindow {
    bool present = false;
    bool known = false;
    float usedPercentage = 0.0f;
    int64_t resetsAt = 0;
};

struct UsageView {
    UsageWindow fiveHour;
    UsageWindow sevenDay;
    uint32_t ageSeconds = 0;
    bool stale = false;
    bool hasData = false;
    UsageWindow included;
    UsageWindow api;
    UsageWindow onDemand;
};

// Providers the firmware can draw. The order is the order on screen and in the
// BOOT button cycle. Supporting a new provider means a new id here, its JSON
// key in api_codec and its name, colours and icon in the renderer.
enum class ProviderId : uint8_t { Codex, Claude, Cursor };

constexpr uint8_t PROVIDER_COUNT = 3;

struct ProviderUsage {
    ProviderId id = ProviderId::Codex;
    UsageView view;
};

// Only the providers the user enabled, in ProviderId order. A provider the
// backend lists but never heard from has hasData == false, never 0%.
struct ProvidersUsage {
    ProviderUsage items[PROVIDER_COUNT];
    uint8_t count = 0;

    const UsageView *find(ProviderId id) const {
        for (uint8_t index = 0; index < count; ++index) {
            if (items[index].id == id) return &items[index].view;
        }
        return nullptr;
    }
};
