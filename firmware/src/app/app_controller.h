#pragma once

#include <cstdint>

#include "state/app_state.h"

void initializeApp(AppState &state);

// Advances the pairing/usage state machine by one tick.
void updateApp(AppState &state, uint32_t nowMs);
