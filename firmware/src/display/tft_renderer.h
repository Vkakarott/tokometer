#pragma once

#if defined(TOKESP_BOARD_ESP32_JD9853) || defined(TOKESP_BOARD_ESP32_C3_JD9853) || \
    defined(TOKESP_DISPLAY_PREVIEW)
#include <Adafruit_GFX.h>
#include "core/usage_types.h"
#include "display/frame_context.h"

void renderTftUsage(Adafruit_GFX &canvas, const ProvidersUsage &usage, uint8_t selectedProvider,
                    bool overview, bool sevenDay, const FrameContext &context);
#endif
