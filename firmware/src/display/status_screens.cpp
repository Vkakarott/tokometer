#include "display/status_screens.h"

#include "core/text_format.h"
#include "display/indexed_canvas.h"

namespace {

using namespace TftColor;

constexpr int16_t TEXT_MARGIN = 16;

// Shrinks the built-in font until the line fits between the margins.
void drawCenteredText(Adafruit_GFX &canvas, const char *text, int16_t y, uint8_t size, uint16_t color) {
    int16_t x1, y1;
    uint16_t width, height;
    canvas.setFont();
    do {
        canvas.setTextSize(size);
        canvas.getTextBounds(text, 0, y, &x1, &y1, &width, &height);
        if (width <= canvas.width() - 2 * TEXT_MARGIN || size <= 1) break;
        --size;
    } while (true);
    canvas.setTextColor(color);
    canvas.setCursor((canvas.width() - width) / 2 - x1, y);
    canvas.print(text);
}

uint32_t pairingSecondsLeft(const AppState &state, uint32_t nowMs) {
    const uint32_t elapsed = (nowMs - state.pairingStartedAtMs) / 1000;
    return elapsed >= state.pairCode.expiresInSeconds ? 0 : state.pairCode.expiresInSeconds - elapsed;
}

void drawWifiScreen(Adafruit_GFX &canvas, WifiStatus status) {
    if (status != WifiStatus::Provisioning) {
        drawCenteredText(canvas, "CONECTANDO", 88, 3, Text);
        drawCenteredText(canvas, "AO WI-FI", 128, 3, Cyan);
        return;
    }
    drawCenteredText(canvas, "CONECTE-SE A", 48, 2, Text);
    drawCenteredText(canvas, provisioningNetworkName(), 84, 3, Cyan);
    drawCenteredText(canvas, "ABRA 192.168.4.1", 142, 2, Text);
    drawCenteredText(canvas, "PARA CONFIGURAR", 174, 2, Muted);
}

void drawPairingScreen(Adafruit_GFX &canvas, const AppState &state, uint32_t nowMs) {
    char code[12];
    char countdown[8];
    formatUserCode(state.pairCode.userCode, code, sizeof code);
    formatCountdown(pairingSecondsLeft(state, nowMs), countdown, sizeof countdown);
    drawCenteredText(canvas, "PAREAMENTO", 42, 2, Text);
    drawCenteredText(canvas, stripUrlScheme(state.pairCode.verificationUri), 76, 2, Text);
    drawCenteredText(canvas, code, 122, 4, Cyan);
    drawCenteredText(canvas, countdown, 190, 2, Muted);
}

void drawErrorScreen(Adafruit_GFX &canvas, const char *message) {
    drawCenteredText(canvas, "ERRO", 80, 3, Orange);
    drawCenteredText(canvas, message, 132, 2, Text);
    drawCenteredText(canvas, "TENTANDO NOVAMENTE...", 176, 1, Muted);
}

}  // namespace

void renderStatusScreen(Adafruit_GFX &canvas, const AppState &state, const FrameContext &context) {
    canvas.fillScreen(Background);
    switch (state.screen) {
        case AppScreen::ConnectingWifi:
            drawWifiScreen(canvas, state.wifiStatus);
            break;
        case AppScreen::RequestingPairCode:
            drawCenteredText(canvas, "PAREAMENTO", 80, 3, Cyan);
            drawCenteredText(canvas, "GERANDO CODIGO...", 132, 2, Text);
            break;
        case AppScreen::WaitingPairApproval:
            drawPairingScreen(canvas, state, context.nowMs);
            break;
        case AppScreen::LoadingUsage:
            drawCenteredText(canvas, "ATUALIZANDO", 94, 3, Cyan);
            drawCenteredText(canvas, "CONSUMO...", 138, 2, Text);
            break;
        case AppScreen::Error:
            drawErrorScreen(canvas, state.errorMessage);
            break;
        case AppScreen::ShowingUsage:
            break;
    }
}
