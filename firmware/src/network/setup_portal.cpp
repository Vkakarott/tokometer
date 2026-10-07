#include "network/setup_portal.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config/wifi_config.h"
#include "network/device_identity.h"

namespace {

constexpr char HTML_TYPE[] = "text/html; charset=utf-8";
constexpr char JSON_TYPE[] = "application/json";
constexpr uint32_t RESCAN_INTERVAL_MS = 30000;
constexpr uint32_t REPLY_GRACE_MS = 400;

constexpr char PAGE_HEAD[] = R"HTML(<!doctype html><html lang="pt-BR"><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>Configurar tokEsp</title><style>body{margin:0;background:#090909;color:#f5f5f5;font:16px -apple-system,BlinkMacSystemFont,sans-serif}.card{max-width:360px;margin:12vh auto;padding:28px;border:1px solid #272727;border-radius:18px;background:#111}h1{margin:0 0 8px;font-size:24px}p{color:#aaa;line-height:1.45}label{display:block;margin:18px 0 6px}input,select,button{box-sizing:border-box;width:100%;border-radius:10px;font:inherit;padding:12px}input,select{background:#191919;border:1px solid #333;color:#fff}button{margin-top:22px;border:0;background:#fff;color:#000;font-weight:650}.hidden{display:none}</style></head><body><main class="card"><h1>Conectar display</h1><p>Escolha uma rede próxima de 2,4 GHz. Para rede aberta, deixe a senha em branco. O display guarda até três redes conhecidas.</p><form method="post" action="/connect"><label>Rede Wi-Fi</label><select name="network" id="network" onchange="document.getElementById('other').classList.toggle('hidden',this.value!=='__other__')"><option value="" disabled selected>Selecione uma rede</option>)HTML";
constexpr char PAGE_TAIL[] = R"HTML(<option value="__other__">Outra rede...</option></select><div id="other" class="hidden"><label>Nome da rede</label><input name="manual_ssid" autocomplete="username" maxlength="32"></div><label>Senha</label><input name="password" type="password" autocomplete="current-password" maxlength="63"><button>Conectar</button></form></main></body></html>)HTML";
constexpr char PAGE_DONE[] = "<meta name=viewport content='width=device-width,initial-scale=1'><body style='background:#090909;color:white;font:16px -apple-system;padding:32px'>Conectando o display. Você já pode voltar ao tokometer.</body>";

DNSServer dnsServer;
WebServer server(80);
bool routesRegistered = false;
bool running = false;

// Scans run in the background: a blocking scan would freeze the display and
// stall the client for several seconds on every page load.
String networkOptions;
bool scanRequested = false;
uint32_t lastScanAtMs = 0;

WifiCredentials pending;
bool hasPending = false;
uint32_t pendingSinceMs = 0;

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

String networkOptionsFrom(int16_t count) {
    String options;
    for (int16_t index = 0; index < count; ++index) {
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
    return options;
}

// A scan takes the radio off the access point's channel, so none starts while
// a client is connected.
void updateNetworkScan() {
    const int16_t count = WiFi.scanComplete();
    if (count >= 0) {
        networkOptions = networkOptionsFrom(count);
        WiFi.scanDelete();
        lastScanAtMs = millis();
        return;
    }
    if (count == WIFI_SCAN_RUNNING || isSetupPortalInUse()) return;
    if (scanRequested && millis() - lastScanAtMs < RESCAN_INTERVAL_MS) return;
    WiFi.scanNetworks(true);
    scanRequested = true;
    lastScanAtMs = millis();
}

void sendHardwareId(int status) {
    char hardwareId[24];
    buildHardwareId(hardwareId, sizeof hardwareId);
    server.send(status, JSON_TYPE, String("{\"hardware_id\":\"") + hardwareId + "\"}");
}

bool acceptCredentials(String ssid, const String &password) {
    ssid.trim();
    if (ssid.isEmpty() || ssid.length() > WifiConfig::MAX_SSID_LENGTH ||
        password.length() > WifiConfig::MAX_PASSWORD_LENGTH) {
        return false;
    }
    pending.ssid = ssid;
    pending.password = password;
    hasPending = true;
    pendingSinceMs = millis();
    return true;
}

void handleApiStatus() {
    sendHardwareId(200);
}

void handleApiWifi() {
    if (!acceptCredentials(server.arg("ssid"), server.arg("password"))) {
        server.send(400, JSON_TYPE, "{\"error\":\"invalid_credentials\"}");
        return;
    }
    sendHardwareId(200);
}

void handlePage() {
    String page = PAGE_HEAD;
    page += networkOptions;
    page += PAGE_TAIL;
    server.send(200, HTML_TYPE, page);
}

void handlePageSubmit() {
    const String ssid = server.arg("network") == "__other__" ? server.arg("manual_ssid") : server.arg("network");
    if (!acceptCredentials(ssid, server.arg("password"))) {
        server.send(400, "text/plain; charset=utf-8", "Dados inválidos. Volte e tente novamente.");
        return;
    }
    server.send(200, HTML_TYPE, PAGE_DONE);
}

void registerRoutes() {
    if (routesRegistered) return;
    server.on("/api/status", HTTP_GET, handleApiStatus);
    server.on("/api/wifi", HTTP_POST, handleApiWifi);
    server.on("/", HTTP_GET, handlePage);
    server.on("/generate_204", HTTP_ANY, handlePage);
    server.on("/hotspot-detect.html", HTTP_ANY, handlePage);
    server.on("/connect", HTTP_POST, handlePageSubmit);
    server.onNotFound(handlePage);
    routesRegistered = true;
}

}  // namespace

void startSetupPortal(const char *accessPointName) {
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(accessPointName)) log_e("could not start the setup access point");
    WiFi.setTxPower(WifiConfig::TX_POWER);
    log_i("setup portal on %s at %s", accessPointName, WiFi.softAPIP().toString().c_str());

    dnsServer.start(53, "*", WiFi.softAPIP());
    registerRoutes();
    server.begin();
    hasPending = false;
    scanRequested = false;
    running = true;
}

void stopSetupPortal() {
    if (!running) return;
    dnsServer.stop();
    server.stop();
    WiFi.scanDelete();
    running = false;
}

bool pollSetupPortal(WifiCredentials &submitted) {
    dnsServer.processNextRequest();
    server.handleClient();
    if (!hasPending) {
        updateNetworkScan();
        return false;
    }
    if (millis() - pendingSinceMs < REPLY_GRACE_MS) return false;
    submitted = pending;
    hasPending = false;
    return true;
}

bool isSetupPortalInUse() {
    return WiFi.softAPgetStationNum() > 0;
}
