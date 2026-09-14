#pragma once

#include <cstdint>

#include "state/app_state.h"

void initializeDisplay();
void drawFrame(const AppState &state, uint32_t nowMs);
