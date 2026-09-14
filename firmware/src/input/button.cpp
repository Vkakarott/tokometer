#include "input/button.h"

#include <Arduino.h>

#include "config/hardware_config.h"

namespace {

int stableLevel = HIGH;
int lastReading = HIGH;
uint32_t lastChangeAtMs = 0;

}  // namespace

void initializeButton() {
    pinMode(HardwareConfig::PRG_BUTTON_PIN, INPUT_PULLUP);
}

bool pollButtonPress(uint32_t nowMs) {
    const int reading = digitalRead(HardwareConfig::PRG_BUTTON_PIN);
    if (reading != lastReading) {
        lastReading = reading;
        lastChangeAtMs = nowMs;
        return false;
    }
    if (reading == stableLevel || nowMs - lastChangeAtMs < HardwareConfig::BUTTON_DEBOUNCE_MS) {
        return false;
    }
    stableLevel = reading;
    return stableLevel == LOW;
}
