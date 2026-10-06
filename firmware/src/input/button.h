#pragma once

#include <cstdint>

enum class ButtonAction { None, ShortPress, LongPress };

void initializeButton();

// Polls the BOOT button. A long press is reported while still held; a short
// press on release.
ButtonAction pollButtonAction(uint32_t nowMs);
