#pragma once

enum class WifiStatus {
    Disconnected,
    Connecting,
    Connected,
    Failed,
    Provisioning
};

void initializeWifi();
WifiStatus updateWifi();
const char *provisioningNetworkName();
