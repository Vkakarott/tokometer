#pragma once

#include <cstddef>
#include <cstdint>

#include "core/usage_types.h"

struct PairCode {
    char deviceCode[64] = "";
    char userCode[16] = "";
    char verificationUri[128] = "";
    uint32_t expiresInSeconds = 0;
    uint32_t intervalSeconds = 0;
};

enum class TokenPollOutcome {
    Approved,
    Pending,
    Expired,
    Denied,
    Failed
};

struct TokenPoll {
    TokenPollOutcome outcome = TokenPollOutcome::Failed;
    char accessToken[96] = "";
};

bool encodePairCodeRequest(const char *hardwareId, char *out, size_t size);
bool encodeTokenRequest(const char *deviceCode, char *out, size_t size);

bool parsePairCode(const char *json, PairCode &out);
TokenPoll parseTokenPoll(int httpStatus, const char *json);
bool parseUsage(const char *json, UsageView &out);
