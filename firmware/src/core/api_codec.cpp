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
    if (strcmp(id, "included") == 0) return &view.included;
    if (strcmp(id, "api") == 0) return &view.api;
    if (strcmp(id, "on_demand") == 0) return &view.onDemand;
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

struct ProviderKey {
    const char *json;
    ProviderId id;
};

// Keys the firmware knows how to draw, in ProviderId order.
constexpr ProviderKey PROVIDER_KEYS[PROVIDER_COUNT] = {
    {"codex", ProviderId::Codex},
    {"claude", ProviderId::Claude},
    {"cursor", ProviderId::Cursor},
};

bool parseProvider(JsonVariantConst json, UsageView &out) {
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

    // A provider missing from the block is disabled, so it takes no space.
    // Keys the firmware cannot draw are ignored.
    ProvidersUsage usage;
    for (const ProviderKey &key : PROVIDER_KEYS) {
        const JsonVariantConst block = providers[key.json];
        if (block.isNull()) continue;
        ProviderUsage &entry = usage.items[usage.count];
        entry.id = key.id;
        if (!parseProvider(block, entry.view)) return false;
        ++usage.count;
    }
    out = usage;
    return true;
}
