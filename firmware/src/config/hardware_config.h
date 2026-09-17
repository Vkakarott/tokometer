#pragma once

// Board pinout, chosen by the PlatformIO environment's build flag.
#if defined(TOKESP_BOARD_ESP32_I2C)
#include "config/board_esp32_i2c.h"
#else
#include "config/board_heltec_v2.h"
#endif
