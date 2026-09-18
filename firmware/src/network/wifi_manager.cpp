#include <DNSServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>

#include "network/wifi_manager.h"

namespace {

constexpr char NAMESPACE[] = "tokesp";
constexpr char WIFI_COUNT_KEY[] = "wifiCount";
constexpr char LEGACY_WIFI_SSID_KEY[] = "wifiSsid";
constexpr char LEGACY_WIFI_PASSWORD_KEY[] = "wifiPass";
constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint8_t MAX_KNOWN_NETWORKS = 3;

DNSServer dnsServer;
WebServer portal(80);
struct WifiProfile {
    char ssid[33] = "";
    char password[65] = "";
};
WifiProfile knownNetworks[MAX_KNOWN_NETWORKS];
uint8_t knownNetworkCount = 0;
uint8_t activeNetworkIndex = 0;
uint8_t attemptedNetworks = 0;
char portalSsid[20] = "tokEsp";
bool provisioning = false;
uint32_t connectionStartedAtMs = 0;

void networkKey(char *out, size_t size, const char *prefix, uint8_t index) {
    snprintf(out, size, "%s%u", prefix, index);
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
            preferences.remove(ssidKey);
            preferences.remove(passwordKey);
        }
    }
    preferences.remove(LEGACY_WIFI_SSID_KEY);
    preferences.remove(LEGACY_WIFI_PASSWORD_KEY);
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
    dnsServer.stop();
    portal.stop();
    WiFi.mode(WIFI_STA);
    WiFi.begin(knownNetworks[index].ssid, knownNetworks[index].password);
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

void connectToAvailableKnownNetwork() {
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
    connectToKnownNetwork(selected >= 0 ? selected : 0);
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
    if (knownNetworkCount == 0 && preferences.getString(LEGACY_WIFI_SSID_KEY, knownNetworks[0].ssid, sizeof knownNetworks[0].ssid) > 0) {
        preferences.getString(LEGACY_WIFI_PASSWORD_KEY, knownNetworks[0].password, sizeof knownNetworks[0].password);
        knownNetworkCount = 1;
    }
    preferences.end();
    if (knownNetworkCount > 0) saveKnownNetworks();
}

String escapedHtml(const String &value) {
    String escaped;
    escaped.reserve(value.length());
    for (const char character : value) {
        switch (character) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += character; break;
        }
    }
    return escaped;
}

String nearbyNetworkOptions() {
    String options;
    const int count = WiFi.scanNetworks();
    for (int index = 0; index < count; ++index) {
        const String ssid = WiFi.SSID(index);
        if (ssid.isEmpty()) continue;

        options += "<option value=\"";
        options += escapedHtml(ssid);
        options += "\">";
        options += escapedHtml(ssid);
        options += " (";
        options += WiFi.RSSI(index);
        options += " dBm)</option>";
    }
    WiFi.scanDelete();
    return options;
}

void sendPortalPage() {
    String page = R"HTML(<!doctype html><html lang="pt-BR"><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>Configurar tokEsp</title><style>body{margin:0;background:#090909;color:#f5f5f5;font:16px -apple-system,BlinkMacSystemFont,sans-serif}.card{max-width:360px;margin:12vh auto;padding:28px;border:1px solid #272727;border-radius:18px;background:#111}h1{margin:0 0 8px;font-size:24px}p{color:#aaa;line-height:1.45}label{display:block;margin:18px 0 6px}input,select,button{box-sizing:border-box;width:100%;border-radius:10px;font:inherit;padding:12px}input,select{background:#191919;border:1px solid #333;color:#fff}button{margin-top:22px;border:0;background:#fff;color:#000;font-weight:650}.hidden{display:none}</style></head><body><main class="card"><h1>Conectar display</h1><p>Escolha uma rede próxima. Para rede aberta, deixe a senha em branco. O display guarda até três redes conhecidas.</p><form method="post" action="/connect"><label>Rede Wi-Fi</label><select name="network" id="network" onchange="document.getElementById('other').classList.toggle('hidden',this.value!=='__other__')"><option value="" disabled selected>Selecione uma rede</option>)HTML";
    page += nearbyNetworkOptions();
    page += R"HTML(<option value="__other__">Outra rede...</option></select><div id="other" class="hidden"><label>Nome da rede</label><input name="manual_ssid" autocomplete="username" maxlength="32"></div><label>Senha</label><input name="password" type="password" autocomplete="current-password" maxlength="63"><button>Conectar</button></form></main></body></html>)HTML";
    portal.send(200, "text/html; charset=utf-8", page);
}

void startProvisioning() {
    if (provisioning) return;

    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(portalSsid, sizeof portalSsid, "tokEsp-%02X%02X", mac[4], mac[5]);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(portalSsid);
    dnsServer.start(53, "*", WiFi.softAPIP());
    portal.on("/", HTTP_GET, sendPortalPage);
    portal.on("/generate_204", HTTP_ANY, sendPortalPage);
    portal.on("/hotspot-detect.html", HTTP_ANY, sendPortalPage);
    portal.on("/connect", HTTP_POST, [] {
        String ssid = portal.arg("network") == "__other__" ? portal.arg("manual_ssid") : portal.arg("network");
        ssid.trim();
        const String password = portal.arg("password");
        if (ssid.isEmpty() || ssid.length() >= sizeof knownNetworks[0].ssid || password.length() >= sizeof knownNetworks[0].password) {
            portal.send(400, "text/plain; charset=utf-8", "Dados inválidos. Volte e tente novamente.");
            return;
        }
        saveWifiCredentials(ssid, password);
        portal.send(200, "text/html; charset=utf-8", "<meta name=viewport content='width=device-width,initial-scale=1'><body style='background:#090909;color:white;font:16px -apple-system;padding:32px'>Conectando o display. Você já pode voltar ao tokometer.</body>");
        connectToKnownNetwork(0);
    });
    portal.onNotFound(sendPortalPage);
    portal.begin();
    provisioning = true;
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
    if (provisioning) {
        dnsServer.processNextRequest();
        portal.handleClient();
        return WifiStatus::Provisioning;
    }

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
