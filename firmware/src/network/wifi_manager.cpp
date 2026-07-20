#include <WiFi.h>
#include "config/secrets.h"
#include "network/wifi_manager.h"

void initializeWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

WifiStatus updateWifi() {
    const wl_status_t status = WiFi.status();
    switch (status) {
        case WL_CONNECTED:
            return WifiStatus::Connected;
        case WL_CONNECT_FAILED:
            return WifiStatus::Failed;
        case WL_CONNECTION_LOST:
            return WifiStatus::Disconnected;
        case WL_DISCONNECTED:
            return WifiStatus::Disconnected;
        default:
            return WifiStatus::Connection;
    }
}