#pragma once

#include <U8g2lib.h>

#include "core/usage_types.h"
#include "display/frame_context.h"
#include "state/app_state.h"

// Draws the chosen usage layout. A single-provider layout falls back to that
// provider's "no data" screen; the comparison shows "sem dados" per row.
void drawUsageScreen(U8G2 &display, UsageLayout layout, const ProvidersUsage &usage, const FrameContext &context);
