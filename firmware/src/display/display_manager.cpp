#include "display/display_manager.h"

#include <cstring>
#include <SPI.h>

#include "assets/speedbike_logo.h"
#include "config/hardware_config.h"

namespace {
constexpr uint32_t SpiFrequency = 20000000;
constexpr uint16_t Width = IndexedCanvas::Width;
constexpr uint16_t Height = IndexedCanvas::Height;
uint8_t lineBuffer[Width * 2];
IndexedCanvas canvas;
char lastTitle[24] = "";
char lastSubtitle[24] = "";

void command(uint8_t value, const uint8_t *data = nullptr, size_t size = 0) {
    SPI.beginTransaction(SPISettings(SpiFrequency, MSBFIRST, SPI_MODE0));
    digitalWrite(HardwareConfig::TFT_CS_PIN, LOW);
    digitalWrite(HardwareConfig::TFT_DC_PIN, LOW);
    SPI.transfer(value);
    if (size != 0) {
        digitalWrite(HardwareConfig::TFT_DC_PIN, HIGH);
        SPI.writeBytes(data, size);
    }
    digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH);
    SPI.endTransaction();
}

void window(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    const uint16_t endX = x + width - 1;
    const uint16_t endY = y + height - 1;
    const uint8_t columns[] = {static_cast<uint8_t>(x >> 8), static_cast<uint8_t>(x), static_cast<uint8_t>(endX >> 8), static_cast<uint8_t>(endX)};
    const uint8_t rows[] = {static_cast<uint8_t>(y >> 8), static_cast<uint8_t>(y), static_cast<uint8_t>(endY >> 8), static_cast<uint8_t>(endY)};
    command(0x2a, columns, sizeof columns);
    command(0x2b, rows, sizeof rows);
    command(0x2c);
}

void beginPixels() { SPI.beginTransaction(SPISettings(SpiFrequency, MSBFIRST, SPI_MODE0)); digitalWrite(HardwareConfig::TFT_CS_PIN, LOW); digitalWrite(HardwareConfig::TFT_DC_PIN, HIGH); }
void endPixels() { digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH); SPI.endTransaction(); }
bool sameText(const char *stored, const char *text) { return strcmp(stored, text ? text : "") == 0; }
void storeText(char *stored, size_t size, const char *text) { strncpy(stored, text ? text : "", size - 1); stored[size - 1] = '\0'; }
void drawCenteredText(const char *text, int16_t y, uint8_t size, uint16_t color) { int16_t x1, y1; uint16_t textWidth, textHeight; canvas.setFont(); canvas.setTextSize(size); canvas.getTextBounds(text, 0, y, &x1, &y1, &textWidth, &textHeight); canvas.setTextColor(color); canvas.setCursor((Width - textWidth) / 2 - x1, y); canvas.print(text); }
void flush() { pushRegion(0, 0, Width, Height); }
}

void initializeDisplay() { pinMode(HardwareConfig::TFT_CS_PIN, OUTPUT); pinMode(HardwareConfig::TFT_DC_PIN, OUTPUT); pinMode(HardwareConfig::TFT_RESET_PIN, OUTPUT); digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH); SPI.begin(HardwareConfig::TFT_SCK_PIN, -1, HardwareConfig::TFT_MOSI_PIN, HardwareConfig::TFT_CS_PIN); digitalWrite(HardwareConfig::TFT_RESET_PIN, LOW); delay(10); digitalWrite(HardwareConfig::TFT_RESET_PIN, HIGH); delay(120); const uint8_t format = 0x55; command(0x3a, &format, 1); const uint8_t memoryAccessControl = 0x28;  // row/column exchange: 296x240 landscape
    command(0x36, &memoryAccessControl, 1); command(0x11); delay(120); command(0x29); }
void showBootLogo() { canvas.fillScreen(TftColor::Background); flush(); window((Width - SpeedbikeLogoWidth) / 2, (Height - SpeedbikeLogoHeight) / 2, SpeedbikeLogoWidth, SpeedbikeLogoHeight); beginPixels(); SPI.writeBytes(SpeedbikeLogoPixels, sizeof SpeedbikeLogoPixels); endPixels(); }
void showStatus(const char *title, const char *subtitle) { if (sameText(lastTitle, title) && sameText(lastSubtitle, subtitle)) return; storeText(lastTitle, sizeof lastTitle, title); storeText(lastSubtitle, sizeof lastSubtitle, subtitle); canvas.fillScreen(TftColor::Background); drawCenteredText(title, subtitle ? 92 : 108, 3, TftColor::Text); if (subtitle) drawCenteredText(subtitle, 136, 2, TftColor::Cyan); flush(); }
IndexedCanvas &gaugeCanvas() { lastTitle[0] = '\0'; lastSubtitle[0] = '\0'; return canvas; }
void pushRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height) { window(x, y, width, height); beginPixels(); for (uint16_t row = y; row < y + height; ++row) { for (uint16_t column = 0; column < width; ++column) { const uint16_t color = canvas.pixelColor(x + column, row); lineBuffer[column * 2] = color >> 8; lineBuffer[column * 2 + 1] = color; } SPI.writeBytes(lineBuffer, width * 2); } endPixels(); }
