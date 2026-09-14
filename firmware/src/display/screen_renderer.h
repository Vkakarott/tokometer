#pragma once

#include <U8g2lib.h>

#include "display/frame_context.h"
#include "state/app_state.h"

// Draws the current screen into the buffer. Does not clear or send it.
void renderScreen(U8G2 &display, const AppState &state, const FrameContext &context);
