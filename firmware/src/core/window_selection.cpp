#include "core/window_selection.h"

bool billsByCycle(ProviderId id) {
    return id == ProviderId::Cursor;
}

bool isWeeklyOnlyByDesign(ProviderId id) {
    return id == ProviderId::AntigravityArgon;
}

bool reportsOnlyWeekly(const ProviderUsage &entry) {
    if (billsByCycle(entry.id)) return false;
    return !entry.view.fiveHour.present && entry.view.sevenDay.present;
}

bool effectiveSecondary(const ProviderUsage &entry, bool secondary) {
    return secondary || (reportsOnlyWeekly(entry) && !isWeeklyOnlyByDesign(entry.id));
}
