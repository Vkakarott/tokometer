#pragma once

#include <cstdint>

namespace HardwareConfig {

constexpr uint8_t OLED_SDA_PIN = 4;
constexpr uint8_t OLED_SCL_PIN = 15;
constexpr uint8_t OLED_RST_PIN = 16;

constexpr bool ENABLE_SERIAL_DEBUG = false;
constexpr bool ENABLE_RX_VISUAL_DIAG = true;

}