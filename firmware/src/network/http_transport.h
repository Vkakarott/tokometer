#pragma once

#include <Arduino.h>

struct HttpResponse {
    int status = -1;  // negative on transport failure
    String body;
};

// Thin seam over HTTPClient: swapping WiFiClient for WiFiClientSecure stays in
// this file.
HttpResponse httpPostJson(const char *path, const char *jsonBody);
HttpResponse httpGetAuthorized(const char *path, const char *bearerToken);
