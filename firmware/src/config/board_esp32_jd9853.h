#pragma once

#include <cstdint>

namespace HardwareConfig {

constexpr uint8_t TFT_SCK_PIN = 18;
constexpr uint8_t TFT_MOSI_PIN = 23;
constexpr uint8_t TFT_CS_PIN = 5;
constexpr uint8_t TFT_DC_PIN = 2;
constexpr uint8_t TFT_RESET_PIN = 4;

constexpr uint8_t PRG_BUTTON_PIN = 0;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

}
