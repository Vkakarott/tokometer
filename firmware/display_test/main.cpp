#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

namespace {

constexpr uint8_t OLED_SDA_PIN = 21;
constexpr uint8_t OLED_SCL_PIN = 22;
constexpr uint8_t OLED_RESET_PIN = U8X8_PIN_NONE;
constexpr uint8_t OLED_I2C_ADDRESS = 0x3C;
constexpr uint32_t PATTERN_INTERVAL_MS = 3000;

// If the panel is SH1106, add -DTOKESP_DISPLAY_SH1106 to the display_test
// build_flags in platformio.ini.
#if defined(TOKESP_DISPLAY_SH1106)
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, OLED_RESET_PIN);
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, OLED_RESET_PIN);
#endif

void drawCalibrationPattern(bool solid) {
    display.clearBuffer();

    if (solid) {
        display.drawBox(0, 0, 128, 64);
    } else {
        display.drawFrame(0, 0, 128, 64);
        display.drawPixel(0, 0);
        display.drawPixel(127, 0);
        display.drawPixel(0, 63);
        display.drawPixel(127, 63);
    }

    display.sendBuffer();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    display.setI2CAddress(OLED_I2C_ADDRESS << 1);
    display.begin();
    drawCalibrationPattern(true);
    Serial.println("display_test: branco por 3s, depois borda por 3s");
}

void loop() {
    static bool solid = true;
    static uint32_t lastChangeMs = millis();

    if (millis() - lastChangeMs >= PATTERN_INTERVAL_MS) {
        lastChangeMs = millis();
        solid = !solid;
        drawCalibrationPattern(solid);
        Serial.println(solid ? "display_test: branco" : "display_test: borda");
    }
}
