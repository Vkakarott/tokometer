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
};
