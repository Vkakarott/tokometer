#include "network/device_identity.h"

#include <Arduino.h>

namespace {

constexpr uint64_t MAC_MASK = 0xFFFFFFFFFFFFULL;

}  // namespace

// All 48 MAC bits: the low 32 alone are mostly the vendor prefix, shared by
// every board from the same batch.
void buildHardwareId(char *out, size_t size) {
    const uint64_t mac = ESP.getEfuseMac() & MAC_MASK;
    snprintf(out, size, "esp32-%012llx", static_cast<unsigned long long>(mac));
}
