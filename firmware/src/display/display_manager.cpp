#include "display/display_manager.h"

#include <U8g2lib.h>
#if defined(TOKESP_BOARD_ESP32_JD9853)
#include <AnimatedGIF.h>
#include <SPI.h>
#else
#include <Wire.h>
#endif

#include "config/hardware_config.h"
#include "display/frame_context.h"
#include "display/screen_renderer.h"
#include "timekeeping/ntp_clock.h"

namespace {

#if defined(TOKESP_BOARD_ESP32_JD9853)

constexpr uint16_t PANEL_WIDTH = 296;
constexpr uint16_t PANEL_HEIGHT = 240;
constexpr uint16_t RENDER_WIDTH = 296;
constexpr uint16_t RENDER_HEIGHT = 148;
constexpr uint16_t RENDER_TOP = (PANEL_HEIGHT - RENDER_HEIGHT) / 2;
constexpr uint16_t BACKGROUND_COLOR = 0x0000;
constexpr uint16_t FOREGROUND_COLOR = 0xFFFF;
constexpr uint32_t SPI_FREQUENCY = 20000000;
constexpr uint32_t FRAME_INTERVAL_MS = 500;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
AnimatedGIF bootAnimation;
uint32_t lastFrameAtMs = 0;

extern const uint8_t bootAnimationStart[] asm("_binary_firmware_assets_agents_pixel_boot_296x240_gif_start");
extern const uint8_t bootAnimationEnd[] asm("_binary_firmware_assets_agents_pixel_boot_296x240_gif_end");

void writeCommand(uint8_t command, const uint8_t *data = nullptr, size_t size = 0) {
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
    digitalWrite(HardwareConfig::TFT_CS_PIN, LOW);
    digitalWrite(HardwareConfig::TFT_DC_PIN, LOW);
    SPI.transfer(command);
    if (size != 0) {
        digitalWrite(HardwareConfig::TFT_DC_PIN, HIGH);
        SPI.writeBytes(data, size);
    }
    digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH);
    SPI.endTransaction();
}

void setAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    const uint16_t xEnd = x + width - 1;
    const uint16_t yEnd = y + height - 1;
    const uint8_t columns[] = {static_cast<uint8_t>(x >> 8), static_cast<uint8_t>(x),
                               static_cast<uint8_t>(xEnd >> 8), static_cast<uint8_t>(xEnd)};
    const uint8_t rows[] = {static_cast<uint8_t>(y >> 8), static_cast<uint8_t>(y),
                            static_cast<uint8_t>(yEnd >> 8), static_cast<uint8_t>(yEnd)};
    writeCommand(0x2A, columns, sizeof columns);
    writeCommand(0x2B, rows, sizeof rows);
    writeCommand(0x2C);
}

void writeSolidColor(uint16_t color, uint32_t pixelCount) {
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
    digitalWrite(HardwareConfig::TFT_CS_PIN, LOW);
    digitalWrite(HardwareConfig::TFT_DC_PIN, HIGH);
    const uint8_t pixel[] = {static_cast<uint8_t>(color >> 8), static_cast<uint8_t>(color)};
    while (pixelCount-- != 0) SPI.writeBytes(pixel, sizeof pixel);
    digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH);
    SPI.endTransaction();
}

void fillScreen(uint16_t color) {
    setAddressWindow(0, 0, PANEL_WIDTH, PANEL_HEIGHT);
    writeSolidColor(color, static_cast<uint32_t>(PANEL_WIDTH) * PANEL_HEIGHT);
}

void writePixelRow(uint16_t x, uint16_t y, const uint8_t *pixels, const uint16_t *palette,
                   uint16_t length) {
    setAddressWindow(x, y, length, 1);
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
    digitalWrite(HardwareConfig::TFT_CS_PIN, LOW);
    digitalWrite(HardwareConfig::TFT_DC_PIN, HIGH);
    for (uint16_t index = 0; index < length; ++index) {
        const uint16_t color = palette[pixels[index]];
        SPI.transfer(color >> 8);
        SPI.transfer(color);
    }
    digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH);
    SPI.endTransaction();
}

void drawBootAnimationLine(GIFDRAW *draw) {
    const int y = draw->iY + draw->y;
    if (y < 0 || y >= PANEL_HEIGHT) return;

    const int first = max(0, -draw->iX);
    const int last = min(draw->iWidth, PANEL_WIDTH - draw->iX);
    int start = first;
    while (start < last) {
        while (start < last && draw->ucHasTransparency && draw->pPixels[start] == draw->ucTransparent) ++start;
        const int end = draw->ucHasTransparency
            ? [&]() {
                int index = start;
                while (index < last && draw->pPixels[index] != draw->ucTransparent) ++index;
                return index;
            }()
            : last;
        if (start < end) {
            writePixelRow(draw->iX + start, y, draw->pPixels + start, draw->pPalette, end - start);
        }
        start = end;
    }
}

void showBootAnimation() {
    bootAnimation.begin(GIF_PALETTE_RGB565_BE);
    const size_t size = bootAnimationEnd - bootAnimationStart;
    if (!bootAnimation.openFLASH(const_cast<uint8_t *>(bootAnimationStart), size, drawBootAnimationLine)) {
        log_e("Could not open the boot animation");
        return;
    }
    while (bootAnimation.playFrame(true, nullptr)) delay(1);
    bootAnimation.close();
}

bool sourcePixel(uint16_t x, uint16_t y) {
    const uint8_t *buffer = display.getBufferPtr();
    return (buffer[x + (y / 8) * 128] & (1U << (y & 7))) != 0;
}

void sendScaledBuffer() {
    setAddressWindow(0, RENDER_TOP, RENDER_WIDTH, RENDER_HEIGHT);

    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
    digitalWrite(HardwareConfig::TFT_CS_PIN, LOW);
    digitalWrite(HardwareConfig::TFT_DC_PIN, HIGH);
    for (uint16_t y = 0; y < RENDER_HEIGHT; ++y) {
        const uint16_t sourceY = y * 64 / RENDER_HEIGHT;
        for (uint16_t x = 0; x < RENDER_WIDTH; ++x) {
            const uint16_t color = sourcePixel(x * 128 / RENDER_WIDTH, sourceY)
                ? FOREGROUND_COLOR : BACKGROUND_COLOR;
            SPI.transfer(color >> 8);
            SPI.transfer(color);
        }
    }
    digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH);
    SPI.endTransaction();
}

#else

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

#endif

}  // namespace

void initializeDisplay() {
#if defined(TOKESP_BOARD_ESP32_JD9853)
    pinMode(HardwareConfig::TFT_CS_PIN, OUTPUT);
    pinMode(HardwareConfig::TFT_DC_PIN, OUTPUT);
    pinMode(HardwareConfig::TFT_RESET_PIN, OUTPUT);
    digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH);
    SPI.begin(HardwareConfig::TFT_SCK_PIN, -1, HardwareConfig::TFT_MOSI_PIN,
              HardwareConfig::TFT_CS_PIN);

    digitalWrite(HardwareConfig::TFT_RESET_PIN, LOW);
    delay(10);
    digitalWrite(HardwareConfig::TFT_RESET_PIN, HIGH);
    delay(120);

    const uint8_t pixelFormat = 0x55;
    const uint8_t memoryAccessControl = 0x28;
    writeCommand(0x3A, &pixelFormat, 1);
    writeCommand(0x36, &memoryAccessControl, 1);
    writeCommand(0x11);
    delay(120);
    writeCommand(0x29);
    delay(50);
    fillScreen(BACKGROUND_COLOR);
    showBootAnimation();
#else
    if (HardwareConfig::LOG_I2C_SCAN_AT_BOOT) logI2CDevices();
    display.setI2CAddress(HardwareConfig::OLED_I2C_ADDRESS << 1);
    display.begin();
    display.clearBuffer();
    display.sendBuffer();
#endif
}

void drawFrame(const AppState &state, uint32_t nowMs) {
    FrameContext context;
    context.nowMs = nowMs;
    context.nowSeconds = currentEpochSeconds();
    formatClockTime(context.clockText, sizeof context.clockText);

    display.clearBuffer();
    renderScreen(display, state, context);
#if defined(TOKESP_BOARD_ESP32_JD9853)
    if (nowMs - lastFrameAtMs >= FRAME_INTERVAL_MS) {
        sendScaledBuffer();
        lastFrameAtMs = nowMs;
    }
#else
    display.sendBuffer();
#endif
}
