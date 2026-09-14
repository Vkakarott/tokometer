#pragma once

#include <U8g2lib.h>
#include <cstdint>

#include "core/api_codec.h"
#include "network/wifi_manager.h"

void drawWifiScreen(U8G2 &display, WifiStatus status);
void drawMessageScreen(U8G2 &display, const char *title, const char *message);
void drawErrorScreen(U8G2 &display, const char *message);
void drawPairingScreen(U8G2 &display, const PairCode &code, uint32_t secondsLeft);
