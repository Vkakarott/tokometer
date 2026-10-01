#include "display/display_manager.h"

#include <Adafruit_GFX.h>
#include <SPI.h>

#include "display/indexed_canvas.h"
#include "display/tft_renderer.h"

#include "config/hardware_config.h"
#include "core/text_format.h"
#include "display/frame_context.h"
#include "timekeeping/ntp_clock.h"

namespace {

constexpr uint16_t PANEL_WIDTH = IndexedCanvas::Width;
constexpr uint16_t PANEL_HEIGHT = IndexedCanvas::Height;

constexpr uint16_t BACKGROUND_COLOR = TftColor::Background;
constexpr uint16_t FOREGROUND_COLOR = TftColor::Text;
constexpr uint16_t PRIMARY_COLOR = TftColor::Cyan;
constexpr uint16_t SECONDARY_COLOR = TftColor::Text;
constexpr uint16_t MUTED_COLOR = TftColor::Muted;
constexpr uint16_t WARNING_COLOR = TftColor::Orange;

constexpr uint32_t SPI_FREQUENCY = 20000000;
constexpr uint32_t FRAME_INTERVAL_MS = 500;

uint32_t lastFrameAtMs = 0;

uint8_t nativeLineBuffer[PANEL_WIDTH * 2];

IndexedCanvas nativeCanvas;


void writeCommand(uint8_t command, const uint8_t *data = nullptr, size_t size = 0) {
    SPI.beginTransaction(
        SPISettings(
            SPI_FREQUENCY,
            MSBFIRST,
            SPI_MODE0
        )
    );

    digitalWrite(HardwareConfig::TFT_CS_PIN, LOW);
    digitalWrite(HardwareConfig::TFT_DC_PIN, LOW);

    SPI.transfer(command);

    if (size != 0) {
        digitalWrite(
            HardwareConfig::TFT_DC_PIN,
            HIGH
        );

        SPI.writeBytes(
            data,
            size
        );
    }

    digitalWrite(
        HardwareConfig::TFT_CS_PIN,
        HIGH
    );

    SPI.endTransaction();
}


void setAddressWindow(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height
) {
    const uint16_t xEnd = x + width - 1;
    const uint16_t yEnd = y + height - 1;

    const uint8_t columns[] = {
        static_cast<uint8_t>(x >> 8),
        static_cast<uint8_t>(x),
        static_cast<uint8_t>(xEnd >> 8),
        static_cast<uint8_t>(xEnd)
    };

    const uint8_t rows[] = {
        static_cast<uint8_t>(y >> 8),
        static_cast<uint8_t>(y),
        static_cast<uint8_t>(yEnd >> 8),
        static_cast<uint8_t>(yEnd)
    };

    writeCommand(
        0x2A,
        columns,
        sizeof columns
    );

    writeCommand(
        0x2B,
        rows,
        sizeof rows
    );

    writeCommand(0x2C);
}


void writeSolidColor(
    uint16_t color,
    uint32_t pixelCount
) {
    SPI.beginTransaction(
        SPISettings(
            SPI_FREQUENCY,
            MSBFIRST,
            SPI_MODE0
        )
    );

    digitalWrite(
        HardwareConfig::TFT_CS_PIN,
        LOW
    );

    digitalWrite(
        HardwareConfig::TFT_DC_PIN,
        HIGH
    );

    const uint8_t pixel[] = {
        static_cast<uint8_t>(color >> 8),
        static_cast<uint8_t>(color)
    };

    while (pixelCount-- != 0) {
        SPI.writeBytes(
            pixel,
            sizeof pixel
        );
    }

    digitalWrite(
        HardwareConfig::TFT_CS_PIN,
        HIGH
    );

    SPI.endTransaction();
}


void fillScreen(uint16_t color) {
    setAddressWindow(
        0,
        0,
        PANEL_WIDTH,
        PANEL_HEIGHT
    );

    writeSolidColor(
        color,
        static_cast<uint32_t>(PANEL_WIDTH) * PANEL_HEIGHT
    );
}


void writePixelRow(
    uint16_t x,
    uint16_t y,
    const uint8_t *pixels,
    const uint16_t *palette,
    uint16_t length
) {
    setAddressWindow(
        x,
        y,
        length,
        1
    );

    SPI.beginTransaction(
        SPISettings(
            SPI_FREQUENCY,
            MSBFIRST,
            SPI_MODE0
        )
    );

    digitalWrite(
        HardwareConfig::TFT_CS_PIN,
        LOW
    );

    digitalWrite(
        HardwareConfig::TFT_DC_PIN,
        HIGH
    );

    for (uint16_t index = 0; index < length; ++index) {
        const uint16_t color =
            palette[pixels[index]];

        SPI.transfer(color >> 8);
        SPI.transfer(color);
    }

    digitalWrite(
        HardwareConfig::TFT_CS_PIN,
        HIGH
    );

    SPI.endTransaction();
}


void drawCenteredText(
    const char *text,
    int16_t y,
    uint8_t size,
    uint16_t color
) {
    int16_t x1;
    int16_t y1;

    uint16_t width;
    uint16_t height;

    nativeCanvas.setFont();

    do {
        nativeCanvas.setTextSize(size);

        nativeCanvas.getTextBounds(
            text,
            0,
            y,
            &x1,
            &y1,
            &width,
            &height
        );

        if (
            width <= PANEL_WIDTH - 32 ||
            size <= 1
        ) {
            break;
        }

        --size;

    } while (true);

    nativeCanvas.setTextColor(color);

    nativeCanvas.setCursor(
        (PANEL_WIDTH - width) / 2 - x1,
        y
    );

    nativeCanvas.print(text);
}


UsageView liveUsageView(
    UsageView view,
    const AppState &state,
    uint32_t nowMs
) {
    view.ageSeconds +=
        (nowMs - state.usageFetchedAtMs) / 1000;

    view.stale =
        view.stale ||
        state.lastFetchFailed;

    return view;
}


uint32_t pairingSecondsLeft(
    const AppState &state,
    uint32_t nowMs
) {
    const uint32_t elapsed =
        (nowMs - state.pairingStartedAtMs) / 1000;

    return elapsed >= state.pairCode.expiresInSeconds
        ? 0
        : state.pairCode.expiresInSeconds - elapsed;
}


void drawNativeUsageScreen(
    const AppState &state,
    const FrameContext &context
) {
    const ProvidersUsage usage{
        liveUsageView(
            state.usage.claude,
            state,
            context.nowMs
        ),

        liveUsageView(
            state.usage.codex,
            state,
            context.nowMs
        ),

        liveUsageView(
            state.usage.cursor,
            state,
            context.nowMs
        ),
    };

    const uint8_t provider =
        state.layout == UsageLayout::ClaudeCards
            ? 1
            : state.layout == UsageLayout::CursorCards
                ? 2
                : 0;

    renderTftUsage(
        nativeCanvas,
        usage,
        provider,
        state.layout == UsageLayout::Comparison,
        state.sevenDay,
        context
    );
}


void drawNativeStatusScreen(
    const AppState &state,
    const FrameContext &context
) {
    nativeCanvas.fillScreen(
        BACKGROUND_COLOR
    );

    switch (state.screen) {

        case AppScreen::ConnectingWifi:

            if (
                state.wifiStatus ==
                WifiStatus::Provisioning
            ) {
                drawCenteredText(
                    "CONECTE-SE A",
                    48,
                    2,
                    SECONDARY_COLOR
                );

                drawCenteredText(
                    provisioningNetworkName(),
                    84,
                    3,
                    PRIMARY_COLOR
                );

                drawCenteredText(
                    "ABRA 192.168.4.1",
                    142,
                    2,
                    FOREGROUND_COLOR
                );

                drawCenteredText(
                    "PARA CONFIGURAR",
                    174,
                    2,
                    MUTED_COLOR
                );

            } else {

                drawCenteredText(
                    "CONECTANDO",
                    88,
                    3,
                    FOREGROUND_COLOR
                );

                drawCenteredText(
                    "AO WI-FI",
                    128,
                    3,
                    PRIMARY_COLOR
                );
            }

            break;


        case AppScreen::RequestingPairCode:

            drawCenteredText(
                "PAREAMENTO",
                80,
                3,
                PRIMARY_COLOR
            );

            drawCenteredText(
                "GERANDO CODIGO...",
                132,
                2,
                SECONDARY_COLOR
            );

            break;


        case AppScreen::WaitingPairApproval: {

            char code[12];
            char countdown[8];

            formatUserCode(
                state.pairCode.userCode,
                code,
                sizeof code
            );

            formatCountdown(
                pairingSecondsLeft(
                    state,
                    context.nowMs
                ),
                countdown,
                sizeof countdown
            );

            drawCenteredText(
                "PAREAMENTO",
                42,
                2,
                SECONDARY_COLOR
            );

            drawCenteredText(
                stripUrlScheme(
                    state.pairCode.verificationUri
                ),
                76,
                2,
                FOREGROUND_COLOR
            );

            drawCenteredText(
                code,
                122,
                4,
                PRIMARY_COLOR
            );

            drawCenteredText(
                countdown,
                190,
                2,
                MUTED_COLOR
            );

            break;
        }


        case AppScreen::LoadingUsage:

            drawCenteredText(
                "ATUALIZANDO",
                94,
                3,
                PRIMARY_COLOR
            );

            drawCenteredText(
                "CONSUMO...",
                138,
                2,
                SECONDARY_COLOR
            );

            break;


        case AppScreen::Error:

            drawCenteredText(
                "ERRO",
                80,
                3,
                WARNING_COLOR
            );

            drawCenteredText(
                state.errorMessage,
                132,
                2,
                FOREGROUND_COLOR
            );

            drawCenteredText(
                "TENTANDO NOVAMENTE...",
                176,
                1,
                MUTED_COLOR
            );

            break;


        case AppScreen::ShowingUsage:
            break;
    }
}


void sendNativeCanvas() {
    setAddressWindow(
        0,
        0,
        PANEL_WIDTH,
        PANEL_HEIGHT
    );

    SPI.beginTransaction(
        SPISettings(
            SPI_FREQUENCY,
            MSBFIRST,
            SPI_MODE0
        )
    );

    digitalWrite(
        HardwareConfig::TFT_CS_PIN,
        LOW
    );

    digitalWrite(
        HardwareConfig::TFT_DC_PIN,
        HIGH
    );

    for (
        uint16_t y = 0;
        y < PANEL_HEIGHT;
        ++y
    ) {

        for (
            uint16_t x = 0;
            x < PANEL_WIDTH;
            ++x
        ) {

            const uint16_t color =
                nativeCanvas.pixelColor(
                    x,
                    y
                );

            nativeLineBuffer[x * 2] =
                color >> 8;

            nativeLineBuffer[x * 2 + 1] =
                color;
        }

        SPI.writeBytes(
            nativeLineBuffer,
            sizeof nativeLineBuffer
        );
    }

    digitalWrite(
        HardwareConfig::TFT_CS_PIN,
        HIGH
    );

    SPI.endTransaction();
}

}  // namespace


void initializeDisplay() {
    pinMode(
        HardwareConfig::TFT_CS_PIN,
        OUTPUT
    );

    pinMode(
        HardwareConfig::TFT_DC_PIN,
        OUTPUT
    );

    pinMode(
        HardwareConfig::TFT_RESET_PIN,
        OUTPUT
    );

    digitalWrite(
        HardwareConfig::TFT_CS_PIN,
        HIGH
    );

    SPI.begin(
        HardwareConfig::TFT_SCK_PIN,
        -1,
        HardwareConfig::TFT_MOSI_PIN,
        HardwareConfig::TFT_CS_PIN
    );


    digitalWrite(
        HardwareConfig::TFT_RESET_PIN,
        LOW
    );

    delay(10);

    digitalWrite(
        HardwareConfig::TFT_RESET_PIN,
        HIGH
    );

    delay(120);


    const uint8_t pixelFormat = 0x55;

    const uint8_t memoryAccessControl = 0x28;

    writeCommand(
        0x3A,
        &pixelFormat,
        1
    );

    writeCommand(
        0x36,
        &memoryAccessControl,
        1
    );

    writeCommand(0x11);

    delay(120);

    writeCommand(0x29);

    delay(50);

    fillScreen(
        BACKGROUND_COLOR
    );
}


void drawFrame(
    const AppState &state,
    uint32_t nowMs
) {
    static AppScreen lastScreen =
        AppScreen::Error;

    static UsageLayout lastLayout =
        UsageLayout::Comparison;

    static bool lastPeriod = false;


    if (
        nowMs - lastFrameAtMs <
            FRAME_INTERVAL_MS &&
        state.screen == lastScreen &&
        state.layout == lastLayout &&
        state.sevenDay == lastPeriod
    ) {
        return;
    }


    lastScreen = state.screen;
    lastLayout = state.layout;
    lastPeriod = state.sevenDay;


    FrameContext context;

    context.nowMs = nowMs;

    context.nowSeconds =
        currentEpochSeconds();

    formatClockTime(
        context.clockText,
        sizeof context.clockText
    );


    if (
        state.screen ==
        AppScreen::ShowingUsage
    ) {
        drawNativeUsageScreen(
            state,
            context
        );

    } else {

        drawNativeStatusScreen(
            state,
            context
        );
    }


    sendNativeCanvas();

    lastFrameAtMs = nowMs;
}