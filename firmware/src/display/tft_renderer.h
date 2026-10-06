#pragma once

#include <Adafruit_GFX.h>
#include "core/usage_types.h"
#include "display/frame_context.h"

void renderTftUsage(Adafruit_GFX &canvas, const ProvidersUsage &usage, uint8_t selectedProvider,
                    bool overview, bool sevenDay, const FrameContext &context);
