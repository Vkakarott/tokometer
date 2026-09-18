#include <DNSServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>

#include "network/wifi_manager.h"

namespace {

constexpr char NAMESPACE[] = "tokesp";
constexpr char WIFI_SSID_KEY[] = "wifiSsid";
constexpr char WIFI_PASSWORD_KEY[] = "wifiPass";
constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;

DNSServer dnsServer;
WebServer portal(80);
char configuredSsid[33] = "";
char configuredPassword[65] = "";
char portalSsid[20] = "tokEsp";
bool provisioning = false;
uint32_t connectionStartedAtMs = 0;

void connectToConfiguredNetwork() {
    provisioning = false;
    dnsServer.stop();
    portal.stop();
    WiFi.mode(WIFI_STA);
    WiFi.begin(configuredSsid, configuredPassword);
    connectionStartedAtMs = millis();
}

void saveWifiCredentials(const String &ssid, const String &password) {
    Preferences preferences;
    preferences.begin(NAMESPACE, false);
    preferences.putString(WIFI_SSID_KEY, ssid);
    preferences.putString(WIFI_PASSWORD_KEY, password);
    preferences.end();

    ssid.toCharArray(configuredSsid, sizeof configuredSsid);
    password.toCharArray(configuredPassword, sizeof configuredPassword);
}

void sendPortalPage() {
    static constexpr char PAGE[] = R"HTML(<!doctype html><html lang="pt-BR"><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>Configurar tokEsp</title><style>body{margin:0;background:#090909;color:#f5f5f5;font:16px -apple-system,BlinkMacSystemFont,sans-serif}.card{max-width:360px;margin:12vh auto;padding:28px;border:1px solid #272727;border-radius:18px;background:#111}h1{margin:0 0 8px;font-size:24px}p{color:#aaa;line-height:1.45}label{display:block;margin:18px 0 6px}input,button{box-sizing:border-box;width:100%;border-radius:10px;font:inherit;padding:12px}input{background:#191919;border:1px solid #333;color:#fff}button{margin-top:22px;border:0;background:#fff;color:#000;font-weight:650}</style></head><body><main class="card"><h1>Conectar display</h1><p>Escolha a rede Wi-Fi que o display deve usar. As credenciais ficam apenas nesta placa.</p><form method="post" action="/connect"><label>Nome da rede</label><input name="ssid" autocomplete="username" required autofocus><label>Senha</label><input name="password" type="password" autocomplete="current-password"><button>Conectar</button></form></main></body></html>)HTML";
    portal.send(200, "text/html; charset=utf-8", PAGE);
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
        String ssid = portal.arg("ssid");
        ssid.trim();
        const String password = portal.arg("password");
        if (ssid.isEmpty() || ssid.length() >= sizeof configuredSsid || password.length() >= sizeof configuredPassword) {
            portal.send(400, "text/plain; charset=utf-8", "Dados inválidos. Volte e tente novamente.");
            return;
        }
        saveWifiCredentials(ssid, password);
        portal.send(200, "text/html; charset=utf-8", "<meta name=viewport content='width=device-width,initial-scale=1'><body style='background:#090909;color:white;font:16px -apple-system;padding:32px'>Conectando o display. Você já pode voltar ao tokometer.</body>");
        connectToConfiguredNetwork();
    });
    portal.onNotFound(sendPortalPage);
    portal.begin();
    provisioning = true;
}

}  // namespace

void initializeWifi() {
    Preferences preferences;
    preferences.begin(NAMESPACE, true);
    const size_t ssidLength = preferences.getString(WIFI_SSID_KEY, configuredSsid, sizeof configuredSsid);
    if (ssidLength > 0) preferences.getString(WIFI_PASSWORD_KEY, configuredPassword, sizeof configuredPassword);
    preferences.end();

    if (ssidLength > 0) {
        connectToConfiguredNetwork();
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
            return WifiStatus::Connected;
        case WL_CONNECT_FAILED:
            startProvisioning();
            return WifiStatus::Failed;
        case WL_CONNECTION_LOST:
            return WifiStatus::Disconnected;
        case WL_DISCONNECTED:
            if (millis() - connectionStartedAtMs >= CONNECT_TIMEOUT_MS) startProvisioning();
            return WifiStatus::Disconnected;
        default:
            return WifiStatus::Connecting;
    }
}

const char *provisioningNetworkName() {
    return portalSsid;
}
