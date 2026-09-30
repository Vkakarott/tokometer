#include "input/button.h"

#include <Arduino.h>

#include "config/hardware_config.h"

namespace {

int stableLevel = HIGH;
int lastReading = HIGH;
uint32_t lastChangeAtMs = 0;
constexpr uint32_t LONG_PRESS_MS = 650;
uint32_t pressedAtMs = 0;
bool pressActive = false;
bool longPressEmitted = false;

bool updateDebouncedLevel(uint32_t nowMs) {
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
    return true;
}

}  // namespace

void initializeButton() {
    pinMode(HardwareConfig::PRG_BUTTON_PIN, INPUT_PULLUP);
}

bool pollButtonPress(uint32_t nowMs) {
    return updateDebouncedLevel(nowMs) && stableLevel == LOW;
}

ButtonAction pollButtonAction(uint32_t nowMs) {
    if (updateDebouncedLevel(nowMs)) {
        if (stableLevel == LOW) {
            pressedAtMs = lastChangeAtMs;
            pressActive = true;
            longPressEmitted = false;
        } else if (pressActive) {
            pressActive = false;
            if (longPressEmitted) return ButtonAction::None;
            return lastChangeAtMs - pressedAtMs >= LONG_PRESS_MS
                ? ButtonAction::LongPress : ButtonAction::ShortPress;
        }
    }
    if (pressActive && !longPressEmitted && lastReading == LOW &&
        nowMs - pressedAtMs >= LONG_PRESS_MS) {
        longPressEmitted = true;
        return ButtonAction::LongPress;
    }
    return ButtonAction::None;
}
