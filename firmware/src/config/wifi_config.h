#pragma once

#include <WiFi.h>

namespace WifiConfig {

// At full power the C3 Mini's antenna fails authentication (AUTH_EXPIRE).
constexpr wifi_power_t TX_POWER = WIFI_POWER_8_5dBm;

constexpr size_t MAX_SSID_LENGTH = 32;
constexpr size_t MAX_PASSWORD_LENGTH = 64;

}
