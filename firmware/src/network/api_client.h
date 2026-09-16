#pragma once

#include "core/api_codec.h"
#include "core/usage_types.h"

enum class UsageFetchStatus {
    Ok,
    Unauthorized,
    Failed
};

bool requestPairCode(PairCode &out);
TokenPoll pollPairToken(const char *deviceCode);
UsageFetchStatus fetchUsage(const char *accessToken, ProvidersUsage &out);
