#pragma once

#include <Arduino.h>

struct WifiCredentials {
    String ssid;
    String password;
};

// Soft access point used to hand the board its Wi-Fi network. The tokometer
// app talks to the JSON endpoints; the captive page is the manual fallback.
//
//   GET  /api/status  -> {"hardware_id":"esp32-..."}
//   POST /api/wifi    (form fields ssid, password) -> {"hardware_id":"esp32-..."}
void startSetupPortal(const char *accessPointName);
void stopSetupPortal();

// Serves pending requests. Returns true once, shortly after a network was
// submitted, so the reply reaches the client before the access point goes away.
bool pollSetupPortal(WifiCredentials &submitted);

bool isSetupPortalInUse();
