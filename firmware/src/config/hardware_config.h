#pragma once

#include <cstdint>

// ESP32-C3 Mini wired to a 240x296 JD9853 SPI TFT. VCC and backlight sit on 3V3.
namespace HardwareConfig {

constexpr uint8_t TFT_SCK_PIN = 6;
constexpr uint8_t TFT_MOSI_PIN = 7;
constexpr uint8_t TFT_CS_PIN = 10;
constexpr uint8_t TFT_DC_PIN = 3;
constexpr uint8_t TFT_RESET_PIN = 1;

// On-board BOOT button, active low. It is also a strapping pin: held down
// during reset it puts the chip in download mode.
constexpr uint8_t BOOT_BUTTON_PIN = 9;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

}
