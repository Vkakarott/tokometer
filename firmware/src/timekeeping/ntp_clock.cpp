#include "timekeeping/ntp_clock.h"

#include <Arduino.h>
#include <ctime>

#include "config/clock_config.h"

void initializeClock() {
    configTzTime(ClockConfig::TIMEZONE, ClockConfig::NTP_PRIMARY, ClockConfig::NTP_SECONDARY);
}

int64_t currentEpochSeconds() {
    const time_t now = time(nullptr);
    return now >= ClockConfig::MIN_VALID_EPOCH ? static_cast<int64_t>(now) : 0;
}

bool formatClockTime(char *out, size_t size) {
    const time_t now = static_cast<time_t>(currentEpochSeconds());
    if (now == 0) return false;

    struct tm local;
    localtime_r(&now, &local);
    return strftime(out, size, "%H:%M", &local) > 0;
}
