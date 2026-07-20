#pragma once

enum class WifiStatus {
    Disconnected,
    Connection,
    Connected,
    Failed
};

void initializeWifi();
WifiStatus updateWifi();