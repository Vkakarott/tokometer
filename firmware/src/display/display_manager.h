#pragma once

#include <cstdint>

#include "state/app_state.h"

// Brings the panel up and clears it.
void initializeDisplay();

// Redraws when the state changed or the current screen is due for a refresh,
// and sends only the rows that differ from what the panel already shows.
void drawFrame(const AppState &state, uint32_t nowMs);
