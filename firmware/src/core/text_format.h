#pragma once

#include <cstddef>
#include <cstdint>

#include "core/usage_types.h"

// 0..1 fill for bars. An absent or reset window renders as an empty bar.
float usageFillFraction(const UsageWindow &window);

// Text of the big number on every usage layout ("62%", or "--%" when unknown).
// Every layout goes through here, so this is the single place to change the
// displayed metric.
void formatPrimaryValue(const UsageWindow &window, char *out, size_t size);

// Time left until the window resets ("2h14", "45min", "6d12h"). Empty when the
// window is unknown or the clock has not synced yet (nowSeconds == 0).
void formatResetIn(const UsageWindow &window, int64_t nowSeconds, char *out, size_t size);

// Age of the data ("agora", "3min", "2h", "2d").
void formatAge(uint32_t ageSeconds, char *out, size_t size);

// "K7QM3F9A" -> "K7QM-3F9A". The hyphen exists only on screen.
void formatUserCode(const char *userCode, char *out, size_t size);

// 299 -> "4:59".
void formatCountdown(uint32_t seconds, char *out, size_t size);

const char *stripUrlScheme(const char *url);
