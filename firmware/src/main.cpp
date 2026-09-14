#include <Arduino.h>
#include <Wire.h>

#include "app/app_controller.h"
#include "config/hardware_config.h"
#include "display/display_manager.h"
#include "input/button.h"
#include "network/wifi_manager.h"
#include "state/app_state.h"
#include "timekeeping/ntp_clock.h"

namespace {

constexpr uint32_t LOOP_INTERVAL_MS = 20;

AppState appState;

}  // namespace

void setup() {
    Serial.begin(115200);

    Wire.begin(
        HardwareConfig::OLED_SDA_PIN,
        HardwareConfig::OLED_SCL_PIN
    );

    initializeDisplay();
    initializeButton();
    initializeWifi();
    initializeClock();
    initializeApp(appState);
}

void loop() {
    const uint32_t nowMs = millis();
    updateApp(appState, nowMs);
    drawFrame(appState, nowMs);
    delay(LOOP_INTERVAL_MS);
}
