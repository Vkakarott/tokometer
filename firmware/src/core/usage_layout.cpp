#include "core/usage_layout.h"

UsageLayout dashboardFor(ProviderId id) {
    return static_cast<UsageLayout>(1 + static_cast<uint8_t>(id));
}

ProviderId providerOf(UsageLayout layout) {
    const uint8_t index = static_cast<uint8_t>(layout);
    return static_cast<ProviderId>(index > 0 ? index - 1 : 0);
}

bool isLayoutAvailable(UsageLayout layout, const ProvidersUsage &usage) {
    if (layout == UsageLayout::Overview) return usage.count >= 2;
    if (static_cast<uint8_t>(layout) >= USAGE_LAYOUT_COUNT) return false;
    return usage.find(providerOf(layout)) != nullptr;
}

UsageLayout resolveLayout(UsageLayout preferred, const ProvidersUsage &usage) {
    if (static_cast<uint8_t>(preferred) >= USAGE_LAYOUT_COUNT) preferred = UsageLayout::Overview;
    if (isLayoutAvailable(preferred, usage)) return preferred;
    return nextLayout(preferred, usage);
}

UsageLayout nextLayout(UsageLayout current, const ProvidersUsage &usage) {
    for (uint8_t step = 1; step <= USAGE_LAYOUT_COUNT; ++step) {
        const auto candidate = static_cast<UsageLayout>((static_cast<uint8_t>(current) + step) % USAGE_LAYOUT_COUNT);
        if (isLayoutAvailable(candidate, usage)) return candidate;
    }
    return UsageLayout::Overview;
}
