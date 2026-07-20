#include "display_manager.h"
#include "../config/hardware_config.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(
    U8G2_R0,
    HardwareConfig::OLED_RST_PIN
);

void initializeDisplay() {
    display.begin();
    display.clearBuffer();
    display.sendBuffer();
}

void drawFrame() {
    display.clearBuffer();

    display.drawFrame(0, 0, 128, 64);

    display.sendBuffer();
}