#pragma once

#include <cstdint>

namespace ClockConfig {

// POSIX TZ string for Brasília time: UTC-3, no daylight saving.
constexpr char TIMEZONE[] = "<-03>3";
constexpr char NTP_PRIMARY[] = "pool.ntp.org";
constexpr char NTP_SECONDARY[] = "time.google.com";

// Anything earlier means SNTP has not synced and time() still counts from boot.
constexpr int64_t MIN_VALID_EPOCH = 1700000000;

}
