#pragma once

#include <cstdint>

namespace SpeedBikeConfig {
constexpr char DeviceName[] = "speedbike_050102";
constexpr uint8_t Password[] = {1, 1, 1, 1, 1, 1};
constexpr uint32_t PasswordRequestTimeoutMs = 15000;
constexpr uint32_t PasswordRequestRetryMs = 2000;
constexpr uint8_t PasswordRequestAttempts = 3;
constexpr uint32_t AuthenticationTimeoutMs = 10000;
constexpr uint32_t ReconnectDelayMs = 2000;
constexpr uint32_t NoDataTimeoutMs = 3000;
constexpr uint16_t MinRpm = 0;
constexpr uint16_t MaxRpm = 12000;
constexpr uint16_t RedZoneRpm = 10000;
}
