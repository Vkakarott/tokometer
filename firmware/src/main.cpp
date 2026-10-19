#include <Arduino.h>

#include "app/app_controller.h"
#include "display/display_manager.h"
#include "input/button.h"
#include "network/host_resolver.h"
#include "network/wifi_manager.h"
#include "state/app_state.h"
#include "timekeeping/ntp_clock.h"

namespace {

constexpr uint32_t LOOP_INTERVAL_MS = 20;
// The lowest frequency the Wi-Fi driver supports; rendering stays well under a frame.
constexpr uint32_t CPU_FREQUENCY_MHZ = 80;

AppState appState;

}  // namespace

void setup() {
    setCpuFrequencyMhz(CPU_FREQUENCY_MHZ);
    Serial.begin(115200);

    initializeDisplay();
    // The welcome screen goes up before anything that can take a while.
    drawFrame(appState, millis());
    initializeButton();
    initializeWifi();
    initializeHostResolver();
    initializeClock();
    initializeApp(appState);
}

void loop() {
    const uint32_t nowMs = millis();
    updateApp(appState, nowMs);
    drawFrame(appState, nowMs);
    delay(LOOP_INTERVAL_MS);
}
