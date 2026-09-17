#pragma once

#include <cstdint>

// ESP32 DevKit (ESP-WROOM-32) with a 4-pin I2C OLED module:
// GND -> GND, VCC -> 3V3, SCL -> D22, SDA -> D21.
namespace HardwareConfig {

constexpr uint8_t OLED_SDA_PIN = 21;
constexpr uint8_t OLED_SCL_PIN = 22;

// The module has no reset line: 255 is U8g2's "no pin".
constexpr uint8_t OLED_RESET_PIN = 255;

// Most modules ship at 0x3C; a few are 0x3D. The boot scan tells which.
constexpr uint8_t OLED_I2C_ADDRESS = 0x3C;

// On-board BOOT button, active low.
constexpr uint8_t PRG_BUTTON_PIN = 0;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

// Bring-up aid on new wiring: logs every address that answers on the bus.
constexpr bool LOG_I2C_SCAN_AT_BOOT = true;

}
