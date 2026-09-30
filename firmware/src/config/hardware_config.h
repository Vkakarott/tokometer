#pragma once

// Board pinout, chosen by the PlatformIO environment's build flag.
#if defined(TOKESP_BOARD_ESP32_JD9853)
#include "config/board_esp32_jd9853.h"
#elif defined(TOKESP_BOARD_ESP32_I2C)
#include "config/board_esp32_i2c.h"
#else
#include "config/board_heltec_v2.h"
#endif
