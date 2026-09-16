#pragma once

#include <Arduino.h>

// Turns API_HOST into an address over mDNS and remembers it. The backend can
// change IP without the firmware being reflashed.
void initializeHostResolver();

// "http://192.168.1.37:43110", or empty while the host cannot be resolved.
String backendBaseUrl();

// Drops the cached address so the next call resolves again.
void forgetResolvedHost();
