#pragma once

#include "core/usage_types.h"

// Cursor bills by plan cycle instead of five-hour and seven-day windows.
bool billsByCycle(ProviderId id);

// Argon's quota is weekly by design. Its 5-hour tab stays selectable and reads
// "Sem dados" instead of silently showing the weekly window.
bool isWeeklyOnlyByDesign(ProviderId id);

// A provider that reports a weekly window but no 5-hour one. Cursor is excluded.
bool reportsOnlyWeekly(const ProviderUsage &entry);

// Whether the secondary window (7D, or API for Cursor) is the one to show:
// the user's choice, or the weekly fallback for the default view. A provider
// that is weekly-only by design never falls back, so its 5H tab stays selectable.
bool effectiveSecondary(const ProviderUsage &entry, bool secondary);
