#pragma once

#include <cstddef>
#include <cstdint>

// Persistent device settings in NVS. The access token lives here, never in
// secrets.h, so it can be rotated without reflashing.
bool loadAccessToken(char *out, size_t size);
void saveAccessToken(const char *token);
void clearAccessToken();

uint8_t loadLayoutIndex();
void saveLayoutIndex(uint8_t index);
