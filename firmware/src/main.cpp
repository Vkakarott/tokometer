#include <Arduino.h>

#include "app/app_controller.h"
#include "display/display_manager.h"

namespace {
constexpr uint32_t LoopIntervalMs = 20;
}

void setup() {
    Serial.begin(115200);
    initializeDisplay();
    initializeApp();
}

void loop() {
    updateApp(millis());
    delay(LoopIntervalMs);
}
