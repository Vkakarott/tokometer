#include "display_manager.h"
#include "../config/hardware_config.h"
#include "assets/wifi_icon.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(
    U8G2_R0,
    HardwareConfig::OLED_RST_PIN
);

static const char* wifiStatusText(WifiStatus status);
static void drawConnectionFrame(WifiStatus status);

void initializeDisplay() {
    display.begin();
    display.clearBuffer();
    display.sendBuffer();
}

void drawFrame(const AppState& appState) {
    display.clearBuffer();
    if (appState.wifiStatus != WifiStatus::Connected) {
        drawConnectionFrame(appState.wifiStatus);
    } else {
        display.drawXBMP(56, 8, 48, 48, epd_bitmap_72264);
    }
    display.sendBuffer();
}

static const char* wifiStatusText(WifiStatus status) {
    switch (status) {
        case WifiStatus::Connected:
            return "WiFi conectado!";
        case WifiStatus::Connecting:
            return "Conectando...";
        case WifiStatus::Disconnected:
            return "WiFi desconectado";
        case WifiStatus::Failed:
            return "Falha no WiFi";
    }

    return "WiFi desconhecido";
}

static void drawConnectionFrame(WifiStatus status) {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(6, 14, wifiStatusText(status));
}