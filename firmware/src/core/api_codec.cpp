#include "core/api_codec.h"

#include <ArduinoJson.h>
#include <cstring>

namespace {

constexpr uint32_t DEFAULT_POLL_INTERVAL_SECONDS = 5;

bool copyBounded(const char *source, char *out, size_t size) {
    if (source == nullptr) return false;
    const size_t length = strlen(source);
    if (length == 0 || length >= size) return false;
    memcpy(out, source, length + 1);
    return true;
}

bool serializeBounded(const JsonDocument &doc, char *out, size_t size) {
    if (measureJson(doc) >= size) return false;
    serializeJson(doc, out, size);
    return true;
}

float clampPercentage(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 100.0f) return 100.0f;
    return value;
}

UsageWindow *windowById(const char *id, UsageView &view) {
    if (id == nullptr) return nullptr;
    if (strcmp(id, "five_hour") == 0) return &view.fiveHour;
    if (strcmp(id, "seven_day") == 0) return &view.sevenDay;
    return nullptr;
}

void parseWindow(JsonObjectConst json, UsageView &view) {
    UsageWindow *window = windowById(json["id"].as<const char *>(), view);
    if (window == nullptr) return;

    const JsonVariantConst percentage = json["usedPercentage"];
    window->present = true;
    window->known = percentage.is<float>();
    window->usedPercentage = window->known ? clampPercentage(percentage.as<float>()) : 0.0f;
    window->resetsAt = json["resetsAt"].as<int64_t>();
}

// An absent provider block reads as "no data yet"; a present one must be valid.
bool parseProvider(JsonVariantConst json, UsageView &out) {
    if (json.isNull()) return true;
    if (!json["hasData"].is<bool>()) return false;

    UsageView view;
    view.hasData = json["hasData"];
    view.stale = json["stale"] | false;
    view.ageSeconds = json["ageSeconds"] | 0u;
    for (JsonObjectConst window : json["windows"].as<JsonArrayConst>()) {
        parseWindow(window, view);
    }
    out = view;
    return true;
}

TokenPollOutcome outcomeForError(const char *error) {
    if (error == nullptr) return TokenPollOutcome::Failed;
    if (strcmp(error, "authorization_pending") == 0) return TokenPollOutcome::Pending;
    if (strcmp(error, "expired_token") == 0) return TokenPollOutcome::Expired;
    if (strcmp(error, "access_denied") == 0) return TokenPollOutcome::Denied;
    return TokenPollOutcome::Failed;
}

}  // namespace

bool encodePairCodeRequest(const char *hardwareId, char *out, size_t size) {
    JsonDocument doc;
    doc["hardware_id"] = hardwareId;
    return serializeBounded(doc, out, size);
}

bool encodeTokenRequest(const char *deviceCode, char *out, size_t size) {
    JsonDocument doc;
    doc["device_code"] = deviceCode;
    return serializeBounded(doc, out, size);
}

bool parsePairCode(const char *json, PairCode &out) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    PairCode code;
    const bool copied =
        copyBounded(doc["device_code"], code.deviceCode, sizeof code.deviceCode) &&
        copyBounded(doc["user_code"], code.userCode, sizeof code.userCode) &&
        copyBounded(doc["verification_uri"], code.verificationUri, sizeof code.verificationUri);
    if (!copied) return false;

    code.expiresInSeconds = doc["expires_in"] | 0u;
    code.intervalSeconds = doc["interval"] | DEFAULT_POLL_INTERVAL_SECONDS;
    if (code.intervalSeconds == 0) code.intervalSeconds = DEFAULT_POLL_INTERVAL_SECONDS;
    out = code;
    return true;
}

TokenPoll parseTokenPoll(int httpStatus, const char *json) {
    TokenPoll poll;
    JsonDocument doc;
    if (deserializeJson(doc, json)) return poll;

    if (httpStatus == 200) {
        if (copyBounded(doc["access_token"], poll.accessToken, sizeof poll.accessToken)) {
            poll.outcome = TokenPollOutcome::Approved;
        }
        return poll;
    }
    if (httpStatus == 400) poll.outcome = outcomeForError(doc["error"]);
    return poll;
}

bool parseUsage(const char *json, ProvidersUsage &out) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    const JsonVariantConst providers = doc["providers"];
    if (!providers.is<JsonObjectConst>()) return false;

    ProvidersUsage usage;
    if (!parseProvider(providers["claude"], usage.claude)) return false;
    if (!parseProvider(providers["codex"], usage.codex)) return false;
    out = usage;
    return true;
}
