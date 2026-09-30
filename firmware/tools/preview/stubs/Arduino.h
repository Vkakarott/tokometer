#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#define PROGMEM
#define pgm_read_byte(address) (*reinterpret_cast<const uint8_t *>(address))
#define pgm_read_word(address) (*reinterpret_cast<const uint16_t *>(address))
#define pgm_read_dword(address) (*reinterpret_cast<const uint32_t *>(address))

class __FlashStringHelper;
using String = std::string;

inline void yield() {}
inline double radians(double degrees) { return degrees * 3.14159265358979323846 / 180.0; }
