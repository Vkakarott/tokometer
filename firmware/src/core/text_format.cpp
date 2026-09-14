#include "core/text_format.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int64_t SECONDS_PER_MINUTE = 60;
constexpr int64_t SECONDS_PER_HOUR = 3600;
constexpr int64_t SECONDS_PER_DAY = 86400;
constexpr size_t USER_CODE_LENGTH = 8;

bool hasValue(const UsageWindow &window) {
    return window.present && window.known;
}

void formatDuration(int64_t seconds, char *out, size_t size) {
    const int days = static_cast<int>(seconds / SECONDS_PER_DAY);
    const int hours = static_cast<int>(seconds % SECONDS_PER_DAY / SECONDS_PER_HOUR);
    const int minutes = static_cast<int>(seconds % SECONDS_PER_HOUR / SECONDS_PER_MINUTE);

    if (days > 0) {
        snprintf(out, size, "%dd%02dh", days, hours);
    } else if (hours > 0) {
        snprintf(out, size, "%dh%02d", hours, minutes);
    } else if (minutes > 0) {
        snprintf(out, size, "%dmin", minutes);
    } else {
        snprintf(out, size, "<1min");
    }
}

}  // namespace

float usageFillFraction(const UsageWindow &window) {
    if (!hasValue(window)) return 0.0f;
    return window.usedPercentage / 100.0f;
}

void formatPrimaryValue(const UsageWindow &window, char *out, size_t size) {
    if (!hasValue(window)) {
        snprintf(out, size, "--%%");
        return;
    }
    snprintf(out, size, "%d%%", static_cast<int>(window.usedPercentage + 0.5f));
}

void formatResetIn(const UsageWindow &window, int64_t nowSeconds, char *out, size_t size) {
    if (!hasValue(window) || nowSeconds <= 0) {
        snprintf(out, size, "%s", "");
        return;
    }
    const int64_t remaining = window.resetsAt - nowSeconds;
    formatDuration(remaining > 0 ? remaining : 0, out, size);
}

void formatAge(uint32_t ageSeconds, char *out, size_t size) {
    if (ageSeconds < SECONDS_PER_MINUTE) {
        snprintf(out, size, "agora");
    } else if (ageSeconds < SECONDS_PER_HOUR) {
        snprintf(out, size, "%umin", static_cast<unsigned>(ageSeconds / SECONDS_PER_MINUTE));
    } else if (ageSeconds < SECONDS_PER_DAY) {
        snprintf(out, size, "%uh", static_cast<unsigned>(ageSeconds / SECONDS_PER_HOUR));
    } else {
        snprintf(out, size, "%ud", static_cast<unsigned>(ageSeconds / SECONDS_PER_DAY));
    }
}

void formatUserCode(const char *userCode, char *out, size_t size) {
    if (strlen(userCode) != USER_CODE_LENGTH) {
        snprintf(out, size, "%s", userCode);
        return;
    }
    snprintf(out, size, "%.4s-%.4s", userCode, userCode + 4);
}

void formatCountdown(uint32_t seconds, char *out, size_t size) {
    snprintf(out, size, "%u:%02u", static_cast<unsigned>(seconds / 60),
             static_cast<unsigned>(seconds % 60));
}

const char *stripUrlScheme(const char *url) {
    const char *separator = strstr(url, "://");
    return separator == nullptr ? url : separator + 3;
}
