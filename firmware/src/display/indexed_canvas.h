#pragma once

#if defined(TOKESP_BOARD_ESP32_JD9853) || defined(TOKESP_BOARD_ESP32_C3_JD9853) || \
    defined(TOKESP_DISPLAY_PREVIEW)
#include <Adafruit_GFX.h>
#include <cstring>

namespace TftColor {
enum : uint16_t {
    Background, Surface, Track, Border, Text, Muted, Cyan, CyanDim,
    Orange, OrangeDim, Mint, MintDim, Purple, PurpleDim, OrangeHot, DimText
};
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
}
constexpr uint16_t Palette[] = {
    rgb565(2, 8, 15), rgb565(5, 14, 24), rgb565(24, 44, 63), rgb565(16, 59, 84),
    rgb565(233, 245, 255), rgb565(143, 175, 199), rgb565(0, 208, 255), rgb565(0, 105, 159),
    rgb565(255, 186, 66), rgb565(126, 65, 25), rgb565(42, 244, 195), rgb565(11, 107, 90),
    rgb565(199, 142, 255), rgb565(92, 51, 134), rgb565(255, 119, 52), rgb565(73, 106, 133)
};
}

// Drawing uses palette indices. RGB565 is expanded one scanline at a time for SPI.
class IndexedCanvas : public Adafruit_GFX {
public:
    static constexpr uint16_t Width = 296;
    static constexpr uint16_t Height = 240;

    IndexedCanvas() : Adafruit_GFX(Width, Height) { setTextWrap(false); }

    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x < 0 || y < 0 || x >= Width || y >= Height) return;
        const uint32_t pixel = static_cast<uint32_t>(y) * Width + x;
        const uint8_t shift = (1 - pixel % 2) * 4;
        buffer_[pixel / 2] = (buffer_[pixel / 2] & ~(0x0f << shift)) | ((color & 0x0f) << shift);
    }

    void fillScreen(uint16_t color) override {
        const uint8_t index = color & 0x0f;
        memset(buffer_, index | (index << 4), sizeof buffer_);
    }

    uint16_t pixelColor(uint16_t x, uint16_t y) const {
        if (x >= Width || y >= Height) return 0;
        const uint32_t pixel = static_cast<uint32_t>(y) * Width + x;
        const uint8_t index = (buffer_[pixel / 2] >> ((1 - pixel % 2) * 4)) & 0x0f;
        return TftColor::Palette[index];
    }

private:
    uint8_t buffer_[Width * Height / 2]{};
};
#endif
