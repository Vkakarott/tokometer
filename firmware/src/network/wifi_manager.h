#pragma once

enum class WifiStatus {
    Disconnected,
    Connecting,
    Connected,
    Failed
};

void initializeWifi();
WifiStatus updateWifi();