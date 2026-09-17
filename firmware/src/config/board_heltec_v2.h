#pragma once

#include <cstdint>

// Heltec WiFi LoRa 32 V2: OLED soldered to the board, with a reset line.
namespace HardwareConfig {

constexpr uint8_t OLED_SDA_PIN = 4;
constexpr uint8_t OLED_SCL_PIN = 15;
constexpr uint8_t OLED_RESET_PIN = 16;
constexpr uint8_t OLED_I2C_ADDRESS = 0x3C;

// On-board PRG button, active low.
constexpr uint8_t PRG_BUTTON_PIN = 0;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

constexpr bool LOG_I2C_SCAN_AT_BOOT = false;

}
