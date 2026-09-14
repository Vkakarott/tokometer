#pragma once

#include <cstddef>
#include <cstdint>

// Starts SNTP. Call after WiFi is initialized; syncing happens once it connects.
void initializeClock();

// Unix time in seconds, or 0 while SNTP has not synced.
int64_t currentEpochSeconds();

// Local "HH:MM". Returns false (and leaves `out` untouched) while unsynced.
bool formatClockTime(char *out, size_t size);
