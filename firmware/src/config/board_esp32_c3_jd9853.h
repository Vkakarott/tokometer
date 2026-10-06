#pragma once

#include <cstdint>

namespace HardwareConfig {

constexpr uint8_t TFT_SCK_PIN = 6;
constexpr uint8_t TFT_MOSI_PIN = 7;
constexpr uint8_t TFT_CS_PIN = 10;
constexpr uint8_t TFT_DC_PIN = 3;
constexpr uint8_t TFT_RESET_PIN = 1;

constexpr uint8_t PRG_BUTTON_PIN = 9;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

}
