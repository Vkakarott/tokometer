#include "display/jd9853_panel.h"

#include <Arduino.h>
#include <SPI.h>

#include "config/hardware_config.h"

namespace {

constexpr uint16_t PANEL_WIDTH = IndexedCanvas::Width;
constexpr uint16_t PANEL_HEIGHT = IndexedCanvas::Height;
constexpr uint32_t SPI_FREQUENCY = 20000000;

constexpr uint8_t SLEEP_OUT = 0x11;
constexpr uint8_t DISPLAY_ON = 0x29;
constexpr uint8_t COLUMN_ADDRESS_SET = 0x2A;
constexpr uint8_t ROW_ADDRESS_SET = 0x2B;
constexpr uint8_t MEMORY_WRITE = 0x2C;
constexpr uint8_t MEMORY_ACCESS_CONTROL = 0x36;
constexpr uint8_t PIXEL_FORMAT = 0x3A;

uint8_t lineBuffer[PANEL_WIDTH * 2];

void beginTransfer(uint8_t dataCommandLevel) {
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
    digitalWrite(HardwareConfig::TFT_CS_PIN, LOW);
    digitalWrite(HardwareConfig::TFT_DC_PIN, dataCommandLevel);
}

void endTransfer() {
    digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH);
    SPI.endTransaction();
}

void writeCommand(uint8_t command, const uint8_t *data = nullptr, size_t size = 0) {
    beginTransfer(LOW);
    SPI.transfer(command);
    if (size != 0) {
        digitalWrite(HardwareConfig::TFT_DC_PIN, HIGH);
        SPI.writeBytes(data, size);
    }
    endTransfer();
}

void setAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    const uint16_t xEnd = x + width - 1;
    const uint16_t yEnd = y + height - 1;
    const uint8_t columns[] = {static_cast<uint8_t>(x >> 8), static_cast<uint8_t>(x),
                               static_cast<uint8_t>(xEnd >> 8), static_cast<uint8_t>(xEnd)};
    const uint8_t rows[] = {static_cast<uint8_t>(y >> 8), static_cast<uint8_t>(y),
                            static_cast<uint8_t>(yEnd >> 8), static_cast<uint8_t>(yEnd)};
    writeCommand(COLUMN_ADDRESS_SET, columns, sizeof columns);
    writeCommand(ROW_ADDRESS_SET, rows, sizeof rows);
    writeCommand(MEMORY_WRITE);
}

// The panel takes RGB565 high byte first.
uint8_t *appendColor(uint8_t *out, uint16_t color) {
    *out++ = color >> 8;
    *out++ = color;
    return out;
}

void resetPanel() {
    digitalWrite(HardwareConfig::TFT_RESET_PIN, LOW);
    delay(10);
    digitalWrite(HardwareConfig::TFT_RESET_PIN, HIGH);
    delay(120);
}

}  // namespace

void initializePanel() {
    pinMode(HardwareConfig::TFT_CS_PIN, OUTPUT);
    pinMode(HardwareConfig::TFT_DC_PIN, OUTPUT);
    pinMode(HardwareConfig::TFT_RESET_PIN, OUTPUT);
    digitalWrite(HardwareConfig::TFT_CS_PIN, HIGH);
    SPI.begin(HardwareConfig::TFT_SCK_PIN, -1, HardwareConfig::TFT_MOSI_PIN,
              HardwareConfig::TFT_CS_PIN);
    resetPanel();

    const uint8_t rgb565 = 0x55;
    const uint8_t landscape = 0x28;
    writeCommand(PIXEL_FORMAT, &rgb565, 1);
    writeCommand(MEMORY_ACCESS_CONTROL, &landscape, 1);
    writeCommand(SLEEP_OUT);
    delay(120);
    writeCommand(DISPLAY_ON);
    delay(50);
}

void fillPanel(uint16_t color) {
    uint8_t *out = lineBuffer;
    for (uint16_t x = 0; x < PANEL_WIDTH; ++x) out = appendColor(out, color);

    setAddressWindow(0, 0, PANEL_WIDTH, PANEL_HEIGHT);
    beginTransfer(HIGH);
    for (uint16_t y = 0; y < PANEL_HEIGHT; ++y) SPI.writeBytes(lineBuffer, sizeof lineBuffer);
    endTransfer();
}

void sendCanvasRows(const IndexedCanvas &canvas, uint16_t firstRow, uint16_t rowCount) {
    if (rowCount == 0 || firstRow + rowCount > PANEL_HEIGHT) return;

    setAddressWindow(0, firstRow, PANEL_WIDTH, rowCount);
    beginTransfer(HIGH);
    for (uint16_t y = firstRow; y < firstRow + rowCount; ++y) {
        const uint8_t *packed = canvas.row(y);
        uint8_t *out = lineBuffer;
        for (uint16_t index = 0; index < IndexedCanvas::RowBytes; ++index) {
            out = appendColor(out, TftColor::Palette[packed[index] >> 4]);
            out = appendColor(out, TftColor::Palette[packed[index] & 0x0f]);
        }
        SPI.writeBytes(lineBuffer, sizeof lineBuffer);
    }
    endTransfer();
}
