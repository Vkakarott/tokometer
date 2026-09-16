#pragma once

#include <cstdint>

// Resolved over mDNS, so a new DHCP lease never means reflashing the board.
// Use the machine's Bonjour name (macOS: System Settings > General > Sharing,
// or `scutil --get LocalHostName`), not an IP.
constexpr char API_HOST[] = "MacBook-Pro-Triunfo.local";

// High port on purpose: nothing else on the machine tends to claim it.
constexpr uint16_t API_PORT = 43110;

namespace ApiConfig {

constexpr uint32_t HTTP_CONNECT_TIMEOUT_MS = 3000;
constexpr uint16_t HTTP_READ_TIMEOUT_MS = 5000;
constexpr uint32_t USAGE_POLL_INTERVAL_MS = 30000;
constexpr uint32_t RETRY_DELAY_MS = 10000;
constexpr uint32_t MDNS_TIMEOUT_MS = 3000;

}
