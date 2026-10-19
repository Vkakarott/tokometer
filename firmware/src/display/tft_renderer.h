#pragma once

#include <Adafruit_GFX.h>
#include "core/usage_layout.h"
#include "core/usage_types.h"
#include "display/frame_context.h"

// `layout` must already be resolved against `usage` (see resolveLayout).
void renderTftUsage(Adafruit_GFX &canvas, const ProvidersUsage &usage, UsageLayout layout,
                    bool sevenDay, const FrameContext &context);
