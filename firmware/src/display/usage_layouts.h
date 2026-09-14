#pragma once

#include <U8g2lib.h>

#include "core/usage_types.h"
#include "display/frame_context.h"
#include "state/app_state.h"

// Draws the chosen usage layout, or the "no data" screen when hasData is false.
void drawUsageScreen(U8G2 &display, UsageLayout layout, const UsageView &usage, const FrameContext &context);
