#include "network/wifi_manager.h"

#include <Preferences.h>
#include <WiFi.h>

#include "config/wifi_config.h"
#include "network/setup_portal.h"

namespace {

constexpr char NAMESPACE[] = "tokesp";
constexpr char WIFI_COUNT_KEY[] = "wifiCount";
constexpr char LEGACY_WIFI_SSID_KEY[] = "wifiSsid";
constexpr char LEGACY_WIFI_PASSWORD_KEY[] = "wifiPass";
constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint8_t MAX_KNOWN_NETWORKS = 3;
// A router reboot or a weak moment must not leave the board waiting for setup.
constexpr uint32_t PORTAL_RETRY_INTERVAL_MS = 60000;

struct WifiProfile {
    char ssid[WifiConfig::MAX_SSID_LENGTH + 1] = "";
    char password[WifiConfig::MAX_PASSWORD_LENGTH + 1] = "";
};
WifiProfile knownNetworks[MAX_KNOWN_NETWORKS];
uint8_t knownNetworkCount = 0;
uint8_t activeNetworkIndex = 0;
uint8_t attemptedNetworks = 0;
char portalSsid[20] = "tokEsp";
bool provisioning = false;
uint32_t connectionStartedAtMs = 0;
uint32_t provisioningStartedAtMs = 0;

void startProvisioning();

void networkKey(char *out, size_t size, const char *prefix, uint8_t index) {
    snprintf(out, size, "%s%u", prefix, index);
}

void removeIfPresent(Preferences &preferences, const char *key) {
    if (preferences.isKey(key)) preferences.remove(key);
}

void saveKnownNetworks() {
    Preferences preferences;
    preferences.begin(NAMESPACE, false);
    preferences.putUChar(WIFI_COUNT_KEY, knownNetworkCount);
    for (uint8_t index = 0; index < MAX_KNOWN_NETWORKS; ++index) {
        char ssidKey[16];
        char passwordKey[16];
        networkKey(ssidKey, sizeof ssidKey, "wifiSsid", index);
        networkKey(passwordKey, sizeof passwordKey, "wifiPass", index);
        if (index < knownNetworkCount) {
            preferences.putString(ssidKey, knownNetworks[index].ssid);
            preferences.putString(passwordKey, knownNetworks[index].password);
        } else {
            removeIfPresent(preferences, ssidKey);
            removeIfPresent(preferences, passwordKey);
        }
    }
    removeIfPresent(preferences, LEGACY_WIFI_SSID_KEY);
    removeIfPresent(preferences, LEGACY_WIFI_PASSWORD_KEY);
    preferences.end();
}

void promoteActiveNetwork() {
    if (activeNetworkIndex == 0 || activeNetworkIndex >= knownNetworkCount) return;

    const WifiProfile active = knownNetworks[activeNetworkIndex];
    for (uint8_t index = activeNetworkIndex; index > 0; --index) {
        knownNetworks[index] = knownNetworks[index - 1];
    }
    knownNetworks[0] = active;
    activeNetworkIndex = 0;
    saveKnownNetworks();
}

void connectToKnownNetwork(uint8_t index) {
    provisioning = false;
    stopSetupPortal();
    WiFi.mode(WIFI_STA);
    WiFi.begin(knownNetworks[index].ssid, knownNetworks[index].password);
    WiFi.setTxPower(WifiConfig::TX_POWER);
    activeNetworkIndex = index;
    attemptedNetworks |= 1 << index;
    connectionStartedAtMs = millis();
}

void saveWifiCredentials(const String &ssid, const String &password) {
    int existingIndex = -1;
    for (uint8_t index = 0; index < knownNetworkCount; ++index) {
        if (ssid == knownNetworks[index].ssid) {
            existingIndex = index;
            break;
        }
    }

    const uint8_t retainedCount = existingIndex >= 0
        ? knownNetworkCount - 1
        : min<uint8_t>(knownNetworkCount, MAX_KNOWN_NETWORKS - 1);
    for (uint8_t index = retainedCount; index > 0; --index) {
        const uint8_t source = existingIndex >= 0 && index - 1 >= existingIndex ? index : index - 1;
        knownNetworks[index] = knownNetworks[source];
    }
    ssid.toCharArray(knownNetworks[0].ssid, sizeof knownNetworks[0].ssid);
    password.toCharArray(knownNetworks[0].password, sizeof knownNetworks[0].password);
    knownNetworkCount = min<uint8_t>(retainedCount + 1, MAX_KNOWN_NETWORKS);
    activeNetworkIndex = 0;
    attemptedNetworks = 0;
    saveKnownNetworks();
}

// Picks the strongest known network in range. With a single profile there is
// nothing to choose, so the scan is skipped.
void connectToAvailableKnownNetwork() {
    if (knownNetworkCount == 1) {
        connectToKnownNetwork(0);
        return;
    }
    WiFi.mode(WIFI_STA);
    const int count = WiFi.scanNetworks();
    int selected = -1;
    int32_t strongestSignal = -1000;
    for (int scanIndex = 0; scanIndex < count; ++scanIndex) {
        const String ssid = WiFi.SSID(scanIndex);
        for (uint8_t profileIndex = 0; profileIndex < knownNetworkCount; ++profileIndex) {
            if (ssid == knownNetworks[profileIndex].ssid && WiFi.RSSI(scanIndex) > strongestSignal) {
                selected = profileIndex;
                strongestSignal = WiFi.RSSI(scanIndex);
            }
        }
    }
    WiFi.scanDelete();
    if (selected < 0) {
        startProvisioning();
        return;
    }
    connectToKnownNetwork(selected);
}

bool tryNextKnownNetwork() {
    for (uint8_t index = 0; index < knownNetworkCount; ++index) {
        if ((attemptedNetworks & (1 << index)) == 0) {
            connectToKnownNetwork(index);
            return true;
        }
    }
    return false;
}

void loadKnownNetworks() {
    Preferences preferences;
    preferences.begin(NAMESPACE, true);
    knownNetworkCount = min<uint8_t>(preferences.getUChar(WIFI_COUNT_KEY, 0), MAX_KNOWN_NETWORKS);
    for (uint8_t index = 0; index < knownNetworkCount; ++index) {
        char ssidKey[16];
        char passwordKey[16];
        networkKey(ssidKey, sizeof ssidKey, "wifiSsid", index);
        networkKey(passwordKey, sizeof passwordKey, "wifiPass", index);
        if (preferences.getString(ssidKey, knownNetworks[index].ssid, sizeof knownNetworks[index].ssid) == 0) {
            knownNetworkCount = index;
            break;
        }
        preferences.getString(passwordKey, knownNetworks[index].password, sizeof knownNetworks[index].password);
    }
    const bool migrateLegacy = knownNetworkCount == 0 && preferences.isKey(LEGACY_WIFI_SSID_KEY) &&
        preferences.getString(LEGACY_WIFI_SSID_KEY, knownNetworks[0].ssid, sizeof knownNetworks[0].ssid) > 0;
    if (migrateLegacy) {
        preferences.getString(LEGACY_WIFI_PASSWORD_KEY, knownNetworks[0].password, sizeof knownNetworks[0].password);
        knownNetworkCount = 1;
    }
    preferences.end();
    if (migrateLegacy) saveKnownNetworks();
}

void startProvisioning() {
    if (provisioning) return;

    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(portalSsid, sizeof portalSsid, "tokEsp-%02X%02X", mac[4], mac[5]);

    WiFi.disconnect();
    startSetupPortal(portalSsid);
    provisioning = true;
    provisioningStartedAtMs = millis();
}

// Never while someone is connected to the portal: retrying tears it down.
bool shouldRetryKnownNetworks() {
    return knownNetworkCount > 0 && !isSetupPortalInUse() &&
           millis() - provisioningStartedAtMs >= PORTAL_RETRY_INTERVAL_MS;
}

WifiStatus updateProvisioning() {
    WifiCredentials submitted;
    if (pollSetupPortal(submitted)) {
        saveWifiCredentials(submitted.ssid, submitted.password);
        connectToKnownNetwork(0);
        return WifiStatus::Connecting;
    }
    if (shouldRetryKnownNetworks()) {
        log_i("retrying the saved networks");
        attemptedNetworks = 0;
        tryNextKnownNetwork();
        return WifiStatus::Connecting;
    }
    return WifiStatus::Provisioning;
}

}  // namespace

void initializeWifi() {
    loadKnownNetworks();
    if (knownNetworkCount > 0) {
        connectToAvailableKnownNetwork();
    } else {
        startProvisioning();
    }
}

WifiStatus updateWifi() {
    if (provisioning) return updateProvisioning();

    const wl_status_t status = WiFi.status();
    switch (status) {
        case WL_CONNECTED:
            promoteActiveNetwork();
            return WifiStatus::Connected;
        case WL_CONNECT_FAILED:
            if (!tryNextKnownNetwork()) startProvisioning();
            return WifiStatus::Failed;
        case WL_CONNECTION_LOST:
            return WifiStatus::Disconnected;
        case WL_NO_SSID_AVAIL:
        case WL_DISCONNECTED:
            if (millis() - connectionStartedAtMs >= CONNECT_TIMEOUT_MS && !tryNextKnownNetwork()) startProvisioning();
            return WifiStatus::Disconnected;
        default:
            return WifiStatus::Connecting;
    }
}

const char *provisioningNetworkName() {
    return portalSsid;
}
