#pragma once

#include <cstdint>

#include "core/usage_types.h"

// Order is the BOOT button cycle and the index saved in NVS: keep it stable.
// A dashboard's value is 1 + the ProviderId it shows.
enum class UsageLayout : uint8_t {
    Overview,
    CodexDashboard,
    ClaudeDashboard,
    CursorDashboard
};

constexpr uint8_t USAGE_LAYOUT_COUNT = 1 + PROVIDER_COUNT;

UsageLayout dashboardFor(ProviderId id);

// The provider a dashboard shows; only meaningful for a dashboard layout.
ProviderId providerOf(UsageLayout layout);

// The overview needs two providers to compare; with one, its dashboard is the
// whole story. A dashboard needs its provider enabled.
bool isLayoutAvailable(UsageLayout layout, const ProvidersUsage &usage);

// The saved layout while it is still available, otherwise the first one that
// is. With no provider enabled this is the overview, which says so.
UsageLayout resolveLayout(UsageLayout preferred, const ProvidersUsage &usage);

// The layout after `current` in the BOOT cycle, skipping unavailable ones.
UsageLayout nextLayout(UsageLayout current, const ProvidersUsage &usage);
