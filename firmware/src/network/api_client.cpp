#include "network/api_client.h"

#include <Arduino.h>

#include "network/device_identity.h"
#include "network/http_transport.h"

namespace {

constexpr size_t REQUEST_BODY_SIZE = 128;

}  // namespace

bool requestPairCode(PairCode &out) {
    char hardwareId[24];
    char body[REQUEST_BODY_SIZE];
    buildHardwareId(hardwareId, sizeof hardwareId);
    if (!encodePairCodeRequest(hardwareId, body, sizeof body)) return false;

    const HttpResponse response = httpPostJson("/device/code", body);
    if (response.status != 200) return false;
    return parsePairCode(response.body.c_str(), out);
}

TokenPoll pollPairToken(const char *deviceCode) {
    char body[REQUEST_BODY_SIZE];
    if (!encodeTokenRequest(deviceCode, body, sizeof body)) return TokenPoll{};

    const HttpResponse response = httpPostJson("/device/token", body);
    return parseTokenPoll(response.status, response.body.c_str());
}

UsageFetchStatus fetchUsage(const char *accessToken, ProvidersUsage &out) {
    const HttpResponse response = httpGetAuthorized("/usage", accessToken);
    if (response.status == 401) return UsageFetchStatus::Unauthorized;
    if (response.status != 200) return UsageFetchStatus::Failed;
    return parseUsage(response.body.c_str(), out) ? UsageFetchStatus::Ok : UsageFetchStatus::Failed;
}
