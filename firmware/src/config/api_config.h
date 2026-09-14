#pragma once

#include <cstdint>

// LAN IP of the machine running the backend. `localhost` does not work on the ESP32.
constexpr char API_BASE_URL[] = "http://192.168.1.23:8080";

namespace ApiConfig {

constexpr uint32_t HTTP_CONNECT_TIMEOUT_MS = 3000;
constexpr uint16_t HTTP_READ_TIMEOUT_MS = 5000;
constexpr uint32_t USAGE_POLL_INTERVAL_MS = 30000;
constexpr uint32_t RETRY_DELAY_MS = 10000;

}
