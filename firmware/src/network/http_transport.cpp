#include "network/http_transport.h"

#include <HTTPClient.h>
#include <WiFiClient.h>

#include "config/api_config.h"
#include "network/host_resolver.h"

namespace {

HttpResponse send(const char *path, const char *bearerToken, const char *jsonBody) {
    WiFiClient client;
    HTTPClient http;
    HttpResponse response;

    const String baseUrl = backendBaseUrl();
    if (baseUrl.isEmpty()) return response;
    if (!http.begin(client, baseUrl + path)) return response;

    http.setConnectTimeout(ApiConfig::HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(ApiConfig::HTTP_READ_TIMEOUT_MS);
    if (bearerToken != nullptr) http.addHeader("Authorization", String("Bearer ") + bearerToken);

    if (jsonBody != nullptr) {
        http.addHeader("Content-Type", "application/json");
        response.status = http.POST(String(jsonBody));
    } else {
        response.status = http.GET();
    }

    if (response.status > 0) {
        response.body = http.getString();
    } else {
        // The machine may have moved to another address: resolve again next time.
        forgetResolvedHost();
        log_w("%s failed: %s", path, HTTPClient::errorToString(response.status).c_str());
    }
    http.end();
    return response;
}

}  // namespace

HttpResponse httpPostJson(const char *path, const char *jsonBody) {
    return send(path, nullptr, jsonBody);
}

HttpResponse httpGetAuthorized(const char *path, const char *bearerToken) {
    return send(path, bearerToken, nullptr);
}
