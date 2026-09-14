#include "display/display_manager.h"

#include <U8g2lib.h>

#include "config/hardware_config.h"
#include "display/frame_context.h"
#include "display/screen_renderer.h"
#include "timekeeping/ntp_clock.h"

namespace {

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(
    U8G2_R0,
    HardwareConfig::OLED_RST_PIN
);

}  // namespace

void initializeDisplay() {
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
