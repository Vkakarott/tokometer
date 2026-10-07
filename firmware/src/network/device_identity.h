#pragma once

#include <cstddef>

// "esp32-<mac>": stable across boots and unique per board. The backend and the
// tokometer app both use it to tell boards apart.
void buildHardwareId(char *out, size_t size);
