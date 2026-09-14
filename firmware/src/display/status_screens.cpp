#include "display/status_screens.h"

#include <cstdio>

#include "assets/wifi_icon.h"
#include "core/text_format.h"
#include "display/widgets.h"

namespace {

constexpr int SCREEN_WIDTH = 128;
constexpr int CENTER_X = SCREEN_WIDTH / 2;

const char *wifiStatusText(WifiStatus status) {
    switch (status) {
        case WifiStatus::Connected:
            return "WiFi conectado";
        case WifiStatus::Connecting:
            return "Conectando WiFi...";
        case WifiStatus::Disconnected:
            return "WiFi desconectado";
        case WifiStatus::Failed:
            return "Falha no WiFi";
    }
    return "WiFi desconhecido";
}

// Long LAN URLs fall back to the narrow font instead of clipping.
void drawFittedText(U8G2 &display, int baselineY, const char *text) {
    display.setFont(u8g2_font_5x7_tf);
    if (display.getUTF8Width(text) > SCREEN_WIDTH - 2) display.setFont(u8g2_font_4x6_tf);
    drawCenteredText(display, CENTER_X, baselineY, text);
}

}  // namespace

void drawWifiScreen(U8G2 &display, WifiStatus status) {
    display.drawXBMP(CENTER_X - 16, 6, 32, 32, epd_bitmap_72264);
    display.setFont(u8g2_font_6x10_tf);
    drawCenteredText(display, CENTER_X, 52, wifiStatusText(status));
}

void drawMessageScreen(U8G2 &display, const char *title, const char *message) {
    display.setFont(u8g2_font_6x10_tf);
    drawCenteredText(display, CENTER_X, 24, title);
    display.drawHLine(CENTER_X - 30, 29, 60);
    display.setFont(u8g2_font_6x10_tf);
    drawCenteredText(display, CENTER_X, 44, message);
}

void drawErrorScreen(U8G2 &display, const char *message) {
    drawMessageScreen(display, "Erro", message);
    display.setFont(u8g2_font_5x7_tf);
    drawCenteredText(display, CENTER_X, 60, "Tentando novamente...");
}

void drawPairingScreen(U8G2 &display, const PairCode &code, uint32_t secondsLeft) {
    char userCode[16];
    char countdown[8];
    char expires[24];
    formatUserCode(code.userCode, userCode, sizeof userCode);
    formatCountdown(secondsLeft, countdown, sizeof countdown);
    snprintf(expires, sizeof expires, "expira em %s", countdown);

    display.setFont(u8g2_font_5x7_tf);
    drawCenteredText(display, CENTER_X, 7, "Acesse e digite o código");
    drawFittedText(display, 16, stripUrlScheme(code.verificationUri));

    display.drawRFrame(1, 20, SCREEN_WIDTH - 2, 28, 3);
    setFittingFont(display, userCode, SCREEN_WIDTH - 12, {u8g2_font_fub14_tf, u8g2_font_fub11_tf});
    drawCenteredText(display, CENTER_X, 41, userCode);

    display.setFont(u8g2_font_5x7_tf);
    drawCenteredText(display, CENTER_X, 60, expires);
}
