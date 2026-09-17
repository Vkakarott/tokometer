#include "display/display_manager.h"

#include <U8g2lib.h>
#include <Wire.h>

#include "config/hardware_config.h"
#include "display/frame_context.h"
#include "display/screen_renderer.h"
#include "timekeeping/ntp_clock.h"

namespace {

// Modules with the same pinout ship with either controller; the flag picks one.
#if defined(TOKESP_DISPLAY_SH1106)
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, HardwareConfig::OLED_RESET_PIN);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, HardwareConfig::OLED_RESET_PIN);
#endif

// Bring-up aid on new wiring: says whether the panel answers, and at which
// address. Nothing listed means the wiring or the power rail is the problem.
void logI2CDevices() {
    for (uint8_t address = 1; address < 127; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) log_i("I2C device at 0x%02X", address);
    }
}

}  // namespace

void initializeDisplay() {
    if (HardwareConfig::LOG_I2C_SCAN_AT_BOOT) logI2CDevices();
    display.setI2CAddress(HardwareConfig::OLED_I2C_ADDRESS << 1);
    display.begin();
    display.clearBuffer();
    display.sendBuffer();
}

void drawFrame(const AppState &state, uint32_t nowMs) {
    FrameContext context;
    context.nowMs = nowMs;
    context.nowSeconds = currentEpochSeconds();
    formatClockTime(context.clockText, sizeof context.clockText);

    display.clearBuffer();
    renderScreen(display, state, context);
    display.sendBuffer();
}
