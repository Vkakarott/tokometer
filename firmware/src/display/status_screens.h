#pragma once

#include <Adafruit_GFX.h>

#include "display/frame_context.h"
#include "state/app_state.h"

// Draws every screen other than the usage dashboard.
void renderStatusScreen(Adafruit_GFX &canvas, const AppState &state, const FrameContext &context);
