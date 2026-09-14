#pragma once

#include <cstdint>

// Per-frame inputs that do not live in AppState.
struct FrameContext {
    uint32_t nowMs = 0;
    int64_t nowSeconds = 0;  // 0 while the clock has not synced
    char clockText[6] = "";  // "HH:MM", empty while the clock has not synced
};
