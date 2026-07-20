#include <Arduino.h>
#include <Wire.h>

#include "config/hardware_config.h"
#include "display/display_manager.h"
#include "network/wifi_manager.h"
#include "state/app_state.h"

AppState appState;

void setup() {
    Serial.begin(115200);

    Wire.begin(
        HardwareConfig::OLED_SDA_PIN,
        HardwareConfig::OLED_SCL_PIN
    );

    initializeWifi();
    initializeDisplay();
}

void loop() {
    const WifiStatus status = updateWifi();
    appState.wifiStatus = status;
    drawFrame(appState);
    delay(100);
}