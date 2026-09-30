#include "display/display_manager.h"

#include <U8g2lib.h>
#if defined(TOKESP_BOARD_ESP32_JD9853)
#include <Adafruit_GFX.h>
#include <AnimatedGIF.h>
#include <SPI.h>
#include <cstring>
#else
#include <Wire.h>
#endif

#include "config/hardware_config.h"
#include "core/text_format.h"
#include "display/frame_context.h"
#include "display/screen_renderer.h"
#include "timekeeping/ntp_clock.h"

namespace {

#if defined(TOKESP_BOARD_ESP32_JD9853)

constexpr uint16_t PANEL_WIDTH = 296;
constexpr uint16_t PANEL_HEIGHT = 240;
constexpr uint16_t BACKGROUND_COLOR = 0x0000;
constexpr uint16_t FOREGROUND_COLOR = 0xFFFF;
constexpr uint16_t SURFACE_COLOR = 0x10A2;
constexpr uint16_t BORDER_COLOR = 0x2945;
constexpr uint16_t PRIMARY_COLOR = 0xA7FF;
constexpr uint16_t SECONDARY_COLOR = 0xBDF7;
constexpr uint16_t MUTED_COLOR = 0x8410;
constexpr uint16_t WARNING_COLOR = 0xFD20;
constexpr uint32_t SPI_FREQUENCY = 20000000;
constexpr uint32_t FRAME_INTERVAL_MS = 500;
constexpr uint32_t PANEL_PIXELS = static_cast<uint32_t>(PANEL_WIDTH) * PANEL_HEIGHT;
constexpr uint32_t NATIVE_FRAME_BUFFER_SIZE = (PANEL_PIXELS + 3) / 4;

AnimatedGIF bootAnimation;
uint32_t lastFrameAtMs = 0;
uint8_t nativeFrameBuffer[NATIVE_FRAME_BUFFER_SIZE];
uint8_t nativeLineBuffer[PANEL_WIDTH * 2];
constexpr uint16_t NATIVE_PALETTE[] = {
    BACKGROUND_COLOR,
    SURFACE_COLOR,
    PRIMARY_COLOR,
    FOREGROUND_COLOR,
};

uint8_t nativeColorIndex(uint16_t color) {
    if (color == BACKGROUND_COLOR) return 0;
    if (color == SURFACE_COLOR || color == BORDER_COLOR || color == MUTED_COLOR) return 1;
    if (color == PRIMARY_COLOR || color == WARNING_COLOR) return 2;
    return 3;
}

class NativeCanvas : public Adafruit_GFX {
public:
    NativeCanvas() : Adafruit_GFX(PANEL_WIDTH, PANEL_HEIGHT) {}

    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x < 0 || x >= PANEL_WIDTH || y < 0 || y >= PANEL_HEIGHT) return;
        const uint32_t pixel = static_cast<uint32_t>(y) * PANEL_WIDTH + x;
        const uint32_t byte = pixel / 4;
        const uint8_t shift = (3 - pixel % 4) * 2;
        nativeFrameBuffer[byte] = (nativeFrameBuffer[byte] & ~(0x03 << shift)) |
                                  (nativeColorIndex(color) << shift);
    }

    void fillScreen(uint16_t color) override {
        const uint8_t index = nativeColorIndex(color);
        memset(nativeFrameBuffer, index * 0x55, sizeof nativeFrameBuffer);
    }
};

NativeCanvas nativeCanvas;

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
    if (draw->ucDisposalMethod == 2) {
        for (int index = first; index < last; ++index) {
            if (draw->pPixels[index] == draw->ucTransparent) draw->pPixels[index] = draw->ucBackground;
        }
        draw->ucHasTransparency = 0;
    }

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
    bootAnimation.begin(GIF_PALETTE_RGB565_LE);
    const size_t size = bootAnimationEnd - bootAnimationStart;
    if (!bootAnimation.openFLASH(const_cast<uint8_t *>(bootAnimationStart), size, drawBootAnimationLine)) {
        log_e("Could not open the boot animation");
        return;
    }
    while (bootAnimation.playFrame(true, nullptr)) delay(1);
    bootAnimation.close();
}

void drawCenteredText(const char *text, int16_t y, uint8_t size, uint16_t color) {
    int16_t x1;
    int16_t y1;
    uint16_t width;
    uint16_t height;
    nativeCanvas.setTextSize(size);
    nativeCanvas.getTextBounds(text, 0, y, &x1, &y1, &width, &height);
    nativeCanvas.setTextColor(color);
    nativeCanvas.setCursor((PANEL_WIDTH - width) / 2, y);
    nativeCanvas.print(text);
}

void drawProgressBar(int16_t x, int16_t y, int16_t width, float fraction, uint16_t color) {
    nativeCanvas.drawRoundRect(x, y, width, 10, 5, BORDER_COLOR);
    const int16_t fill = static_cast<int16_t>((width - 4) * fraction);
    if (fill > 0) nativeCanvas.fillRoundRect(x + 2, y + 2, fill, 6, 3, color);
}

void drawUsageCard(int16_t x, int16_t y, int16_t width, int16_t height, const char *title,
                   const UsageWindow &window, int64_t nowSeconds, uint16_t color) {
    nativeCanvas.fillRoundRect(x, y, width, height, 12, SURFACE_COLOR);
    nativeCanvas.drawRoundRect(x, y, width, height, 12, BORDER_COLOR);
    nativeCanvas.setTextColor(SECONDARY_COLOR);
    nativeCanvas.setTextSize(2);
    nativeCanvas.setCursor(x + 16, y + 15);
    nativeCanvas.print(title);

    if (!window.present) {
        nativeCanvas.setTextColor(MUTED_COLOR);
        nativeCanvas.setTextSize(2);
        nativeCanvas.setCursor(x + 16, y + 62);
        nativeCanvas.print("SEM DADOS");
        return;
    }

    char value[8];
    char reset[12];
    formatPrimaryValue(window, value, sizeof value);
    formatResetIn(window, nowSeconds, reset, sizeof reset);
    nativeCanvas.setTextColor(color);
    nativeCanvas.setTextSize(4);
    nativeCanvas.setCursor(x + 16, y + 48);
    nativeCanvas.print(value);
    drawProgressBar(x + 16, y + height - 43, width - 32, usageFillFraction(window), color);
    nativeCanvas.setTextColor(MUTED_COLOR);
    nativeCanvas.setTextSize(1);
    nativeCanvas.setCursor(x + 16, y + height - 23);
    nativeCanvas.print(reset[0] == '\0' ? "aguardando horario" : reset);
}

UsageView liveUsageView(UsageView view, const AppState &state, uint32_t nowMs) {
    view.ageSeconds += (nowMs - state.usageFetchedAtMs) / 1000;
    view.stale = view.stale || state.lastFetchFailed;
    return view;
}

uint32_t pairingSecondsLeft(const AppState &state, uint32_t nowMs) {
    const uint32_t elapsed = (nowMs - state.pairingStartedAtMs) / 1000;
    return elapsed >= state.pairCode.expiresInSeconds ? 0 : state.pairCode.expiresInSeconds - elapsed;
}

void drawNativeUsageScreen(const AppState &state, const FrameContext &context) {
    const UsageView claude = liveUsageView(state.usage.claude, state, context.nowMs);
    const UsageView codex = liveUsageView(state.usage.codex, state, context.nowMs);
    nativeCanvas.fillScreen(BACKGROUND_COLOR);
    nativeCanvas.setTextColor(FOREGROUND_COLOR);
    nativeCanvas.setTextSize(2);
    nativeCanvas.setCursor(16, 14);
    nativeCanvas.print(state.layout == UsageLayout::ClaudeCards ? "CLAUDE" : "TOKESP");
    nativeCanvas.setTextColor(MUTED_COLOR);
    nativeCanvas.setTextSize(1);
    nativeCanvas.setCursor(247, 18);
    nativeCanvas.print(context.clockText);
    nativeCanvas.drawFastHLine(16, 37, PANEL_WIDTH - 32, BORDER_COLOR);

    if (state.layout == UsageLayout::Comparison) {
        drawUsageCard(16, 54, 128, 164, "CLAUDE", claude.fiveHour, context.nowSeconds, PRIMARY_COLOR);
        drawUsageCard(152, 54, 128, 164, "CODEX", codex.fiveHour, context.nowSeconds, WARNING_COLOR);
        return;
    }

    const UsageView &view = state.layout == UsageLayout::ClaudeCards ? claude : codex;
    const uint16_t color = state.layout == UsageLayout::ClaudeCards ? PRIMARY_COLOR : WARNING_COLOR;
    drawUsageCard(16, 54, 128, 164, "5 HORAS", view.fiveHour, context.nowSeconds, color);
    drawUsageCard(152, 54, 128, 164, "7 DIAS", view.sevenDay, context.nowSeconds, color);
    if (view.stale) {
        nativeCanvas.setTextColor(WARNING_COLOR);
        nativeCanvas.setTextSize(1);
        nativeCanvas.setCursor(16, 228);
        nativeCanvas.print("dados desatualizados");
    }
}

void drawNativeStatusScreen(const AppState &state, const FrameContext &context) {
    nativeCanvas.fillScreen(BACKGROUND_COLOR);
    switch (state.screen) {
        case AppScreen::ConnectingWifi:
            if (state.wifiStatus == WifiStatus::Provisioning) {
                drawCenteredText("CONECTE-SE A", 48, 2, SECONDARY_COLOR);
                drawCenteredText(provisioningNetworkName(), 84, 3, PRIMARY_COLOR);
                drawCenteredText("ABRA 192.168.4.1", 142, 2, FOREGROUND_COLOR);
                drawCenteredText("PARA CONFIGURAR", 174, 2, MUTED_COLOR);
            } else {
                drawCenteredText("CONECTANDO", 88, 3, FOREGROUND_COLOR);
                drawCenteredText("AO WI-FI", 128, 3, PRIMARY_COLOR);
            }
            break;
        case AppScreen::RequestingPairCode:
            drawCenteredText("PAREAMENTO", 80, 3, PRIMARY_COLOR);
            drawCenteredText("GERANDO CODIGO...", 132, 2, SECONDARY_COLOR);
            break;
        case AppScreen::WaitingPairApproval: {
            char code[12];
            char countdown[8];
            formatUserCode(state.pairCode.userCode, code, sizeof code);
            formatCountdown(pairingSecondsLeft(state, context.nowMs), countdown, sizeof countdown);
            drawCenteredText("PAREAMENTO", 42, 2, SECONDARY_COLOR);
            drawCenteredText(stripUrlScheme(state.pairCode.verificationUri), 76, 2, FOREGROUND_COLOR);
            drawCenteredText(code, 122, 4, PRIMARY_COLOR);
            drawCenteredText(countdown, 190, 2, MUTED_COLOR);
            break;
        }
        case AppScreen::LoadingUsage:
            drawCenteredText("ATUALIZANDO", 94, 3, PRIMARY_COLOR);
            drawCenteredText("CONSUMO...", 138, 2, SECONDARY_COLOR);
            break;
        case AppScreen::Error:
            drawCenteredText("ERRO", 80, 3, WARNING_COLOR);
            drawCenteredText(state.errorMessage, 132, 2, FOREGROUND_COLOR);
            drawCenteredText("TENTANDO NOVAMENTE...", 176, 1, MUTED_COLOR);
            break;
        case AppScreen::ShowingUsage:
            break;
    }
}

void sendNativeCanvas() {
    setAddressWindow(0, 0, PANEL_WIDTH, PANEL_HEIGHT);
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
    digitalWrite(HardwareConfig::TFT_CS_PIN, LOW);
    digitalWrite(HardwareConfig::TFT_DC_PIN, HIGH);
    for (uint16_t y = 0; y < PANEL_HEIGHT; ++y) {
        for (uint16_t x = 0; x < PANEL_WIDTH; ++x) {
            const uint32_t pixel = static_cast<uint32_t>(y) * PANEL_WIDTH + x;
            const uint8_t colorIndex = (nativeFrameBuffer[pixel / 4] >> ((3 - pixel % 4) * 2)) & 0x03;
            const uint16_t color = NATIVE_PALETTE[colorIndex];
            nativeLineBuffer[x * 2] = color >> 8;
            nativeLineBuffer[x * 2 + 1] = color;
        }
        SPI.writeBytes(nativeLineBuffer, sizeof nativeLineBuffer);
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
    fillScreen(BACKGROUND_COLOR);
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

#if defined(TOKESP_BOARD_ESP32_JD9853)
    if (state.screen == AppScreen::ShowingUsage) {
        drawNativeUsageScreen(state, context);
    } else {
        drawNativeStatusScreen(state, context);
    }
    if (nowMs - lastFrameAtMs >= FRAME_INTERVAL_MS) {
        sendNativeCanvas();
        lastFrameAtMs = nowMs;
    }
#else
    display.clearBuffer();
    renderScreen(display, state, context);
    display.sendBuffer();
#endif
}
