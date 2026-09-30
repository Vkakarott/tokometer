#pragma once

#include <cstdint>

void initializeButton();

// Polls the PRG button. Returns true once per debounced press.
bool pollButtonPress(uint32_t nowMs);

enum class ButtonAction { None, ShortPress, LongPress };

ButtonAction pollButtonAction(uint32_t nowMs);
