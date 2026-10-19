#pragma once

#include <WiFi.h>

namespace WifiConfig {

// At full power the C3 Mini's antenna fails authentication (AUTH_EXPIRE).
constexpr wifi_power_t TX_POWER = WIFI_POWER_8_5dBm;

// The board only polls the backend, so the radio can sleep across several
// beacons. Ten beacon intervals is about one second at the usual 102.4 ms.
constexpr wifi_ps_type_t POWER_SAVE = WIFI_PS_MAX_MODEM;
constexpr uint16_t LISTEN_INTERVAL_BEACONS = 10;

constexpr size_t MAX_SSID_LENGTH = 32;
constexpr size_t MAX_PASSWORD_LENGTH = 64;

}
