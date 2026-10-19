#include <unity.h>

#include <cstdio>
#include <cstring>

#include "core/api_codec.h"

static const UsageView &view(const ProvidersUsage &usage, ProviderId id) {
    const UsageView *found = usage.find(id);
    TEST_ASSERT_NOT_NULL(found);
    return *found;
}

void setUp() {}
void tearDown() {}

static void test_parse_usage_reads_both_providers() {
    const char *json = R"({"providers":{
        "claude":{"windows":[
            {"id":"five_hour","usedPercentage":23.5,"resetsAt":1738425600},
            {"id":"seven_day","usedPercentage":61,"resetsAt":1738857600}
        ],"ageSeconds":42,"stale":false,"hasData":true,"context":null},
        "codex":{"windows":[
            {"id":"five_hour","usedPercentage":38,"resetsAt":1738443600}
        ],"ageSeconds":10,"stale":true,"hasData":true,"context":null},
        "cursor":{"windows":[
            {"id":"included","usedPercentage":12,"resetsAt":1738990000},
            {"id":"api","usedPercentage":44,"resetsAt":1738990000},
            {"id":"on_demand","usedPercentage":7.5,"resetsAt":1738990000}
        ],"ageSeconds":5,"stale":false,"hasData":true,"context":null}
    }})";
    ProvidersUsage usage;

    TEST_ASSERT_TRUE(parseUsage(json, usage));
    TEST_ASSERT_TRUE(view(usage, ProviderId::Claude).hasData);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Claude).stale);
    TEST_ASSERT_EQUAL_UINT32(42, view(usage, ProviderId::Claude).ageSeconds);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Claude).fiveHour.present && view(usage, ProviderId::Claude).fiveHour.known);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.5f, view(usage, ProviderId::Claude).fiveHour.usedPercentage);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Claude).fiveHour.resetsAt == 1738425600);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 61.0f, view(usage, ProviderId::Claude).sevenDay.usedPercentage);

    TEST_ASSERT_TRUE(view(usage, ProviderId::Codex).hasData);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Codex).stale);
    TEST_ASSERT_EQUAL_UINT32(10, view(usage, ProviderId::Codex).ageSeconds);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.0f, view(usage, ProviderId::Codex).fiveHour.usedPercentage);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Codex).sevenDay.present);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Cursor).hasData);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Cursor).included.present && view(usage, ProviderId::Cursor).included.known);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, view(usage, ProviderId::Cursor).included.usedPercentage);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 44.0f, view(usage, ProviderId::Cursor).api.usedPercentage);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, view(usage, ProviderId::Cursor).onDemand.usedPercentage);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Cursor).included.resetsAt == 1738990000);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Cursor).fiveHour.present);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Cursor).sevenDay.present);
}

static void test_parse_cursor_unknown_and_missing_windows_remain_distinct() {
    const char *json = R"({"providers":{
        "cursor":{"windows":[
            {"id":"included","usedPercentage":null,"resetsAt":1},
            {"id":"on_demand","usedPercentage":null,"resetsAt":1}
        ],"ageSeconds":1200,"stale":true,"hasData":true}}})";
    ProvidersUsage usage;

    TEST_ASSERT_TRUE(parseUsage(json, usage));
    TEST_ASSERT_TRUE(view(usage, ProviderId::Cursor).hasData);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Cursor).stale);
    TEST_ASSERT_EQUAL_UINT32(1200, view(usage, ProviderId::Cursor).ageSeconds);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Cursor).included.present);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Cursor).included.known);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Cursor).onDemand.present);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Cursor).onDemand.known);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Cursor).api.present);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Cursor).fiveHour.present);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Cursor).sevenDay.present);
}

static void test_parse_usage_null_percentage_is_unknown() {
    const char *json = R"({"providers":{
        "claude":{"windows":[{"id":"five_hour","usedPercentage":null,"resetsAt":1}],
            "ageSeconds":0,"stale":false,"hasData":true},
        "codex":{"windows":[],"ageSeconds":0,"stale":false,"hasData":false}}})";
    ProvidersUsage usage;

    TEST_ASSERT_TRUE(parseUsage(json, usage));
    TEST_ASSERT_TRUE(view(usage, ProviderId::Claude).fiveHour.present);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Claude).fiveHour.known);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Codex).hasData);
    TEST_ASSERT_NULL(usage.find(ProviderId::Cursor));
}

static void test_parse_usage_without_data() {
    const char *json = R"({"providers":{
        "claude":{"windows":[],"context":null,"ageSeconds":0,"stale":false,"hasData":false},
        "codex":{"windows":[],"context":null,"ageSeconds":0,"stale":false,"hasData":false}}})";
    ProvidersUsage usage;

    TEST_ASSERT_TRUE(parseUsage(json, usage));
    TEST_ASSERT_FALSE(view(usage, ProviderId::Claude).hasData);
    TEST_ASSERT_FALSE(view(usage, ProviderId::Codex).hasData);
    TEST_ASSERT_NULL(usage.find(ProviderId::Cursor));
}

static void test_parse_usage_ignores_context() {
    const char *json = R"({"providers":{
        "claude":{"windows":[],"ageSeconds":5,"stale":false,"hasData":false,
            "context":{"inputTokens":12500,"outputTokens":2400,"windowSize":200000,"usedPercentage":7.45}},
        "codex":{"windows":[],"ageSeconds":0,"stale":false,"hasData":false}}})";
    ProvidersUsage usage;

    TEST_ASSERT_TRUE(parseUsage(json, usage));
    TEST_ASSERT_EQUAL_UINT32(5, view(usage, ProviderId::Claude).ageSeconds);
}

static void test_parse_usage_clamps_percentage() {
    const char *json = R"({"providers":{
        "codex":{"windows":[{"id":"five_hour","usedPercentage":130,"resetsAt":1}],
            "ageSeconds":0,"stale":false,"hasData":true}}})";
    ProvidersUsage usage;

    TEST_ASSERT_TRUE(parseUsage(json, usage));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, view(usage, ProviderId::Codex).fiveHour.usedPercentage);
}

static void test_parse_usage_missing_provider_block_is_disabled() {
    const char *json = R"({"providers":{
        "claude":{"windows":[{"id":"five_hour","usedPercentage":10,"resetsAt":1}],
            "ageSeconds":0,"stale":false,"hasData":true}}})";
    ProvidersUsage usage;

    TEST_ASSERT_TRUE(parseUsage(json, usage));
    TEST_ASSERT_EQUAL_UINT8(1, usage.count);
    TEST_ASSERT_TRUE(view(usage, ProviderId::Claude).hasData);
    TEST_ASSERT_NULL(usage.find(ProviderId::Codex));
    TEST_ASSERT_NULL(usage.find(ProviderId::Cursor));
}

static void test_parse_usage_lists_providers_in_screen_order() {
    const char *json = R"({"providers":{
        "cursor":{"windows":[],"ageSeconds":0,"stale":false,"hasData":false},
        "gemini":{"windows":[],"ageSeconds":0,"stale":false,"hasData":false},
        "claude":{"windows":[],"ageSeconds":0,"stale":false,"hasData":false},
        "codex":{"windows":[],"ageSeconds":0,"stale":false,"hasData":false}}})";
    ProvidersUsage usage;

    TEST_ASSERT_TRUE(parseUsage(json, usage));
    TEST_ASSERT_EQUAL_UINT8(3, usage.count);
    TEST_ASSERT_TRUE(usage.items[0].id == ProviderId::Codex);
    TEST_ASSERT_TRUE(usage.items[1].id == ProviderId::Claude);
    TEST_ASSERT_TRUE(usage.items[2].id == ProviderId::Cursor);
}

static void test_parse_usage_accepts_no_enabled_provider() {
    ProvidersUsage usage;

    TEST_ASSERT_TRUE(parseUsage(R"({"providers":{}})", usage));
    TEST_ASSERT_EQUAL_UINT8(0, usage.count);
}

static void test_parse_usage_rejects_invalid_payloads() {
    ProvidersUsage usage;

    TEST_ASSERT_FALSE(parseUsage("not json", usage));
    TEST_ASSERT_FALSE(parseUsage(R"({"error":"unauthorized"})", usage));
    TEST_ASSERT_FALSE(parseUsage(R"({"windows":[],"ageSeconds":0,"stale":false,"hasData":false})", usage));
    TEST_ASSERT_FALSE(parseUsage(R"({"providers":{"claude":{"windows":[]}}})", usage));
}

static void test_parse_pair_code() {
    const char *json = R"({"device_code":"4f1c2b1e-uuid","user_code":"K7QM3F9A",
        "verification_uri":"http://tok.local/pair","expires_in":300,"interval":5})";
    PairCode code;

    TEST_ASSERT_TRUE(parsePairCode(json, code));
    TEST_ASSERT_EQUAL_STRING("4f1c2b1e-uuid", code.deviceCode);
    TEST_ASSERT_EQUAL_STRING("K7QM3F9A", code.userCode);
    TEST_ASSERT_EQUAL_STRING("http://tok.local/pair", code.verificationUri);
    TEST_ASSERT_EQUAL_UINT32(300, code.expiresInSeconds);
    TEST_ASSERT_EQUAL_UINT32(5, code.intervalSeconds);
}

static void test_parse_pair_code_defaults_interval() {
    const char *json = R"({"device_code":"d","user_code":"u","verification_uri":"v","expires_in":300})";
    PairCode code;

    TEST_ASSERT_TRUE(parsePairCode(json, code));
    TEST_ASSERT_EQUAL_UINT32(5, code.intervalSeconds);
}

static void test_parse_pair_code_rejects_missing_or_oversized_fields() {
    PairCode code;
    char json[256];
    char longCode[80];
    memset(longCode, 'a', sizeof longCode - 1);
    longCode[sizeof longCode - 1] = '\0';
    snprintf(json, sizeof json, R"({"device_code":"%s","user_code":"u","verification_uri":"v"})", longCode);

    TEST_ASSERT_FALSE(parsePairCode(R"({"device_code":"d","verification_uri":"v"})", code));
    TEST_ASSERT_FALSE(parsePairCode(json, code));
}

static void test_parse_token_poll_approved() {
    const TokenPoll poll = parseTokenPoll(200, R"({"access_token":"abc123","token_type":"Bearer"})");

    TEST_ASSERT_TRUE(poll.outcome == TokenPollOutcome::Approved);
    TEST_ASSERT_EQUAL_STRING("abc123", poll.accessToken);
}

static void test_parse_token_poll_errors() {
    TEST_ASSERT_TRUE(parseTokenPoll(400, R"({"error":"authorization_pending"})").outcome == TokenPollOutcome::Pending);
    TEST_ASSERT_TRUE(parseTokenPoll(400, R"({"error":"expired_token"})").outcome == TokenPollOutcome::Expired);
    TEST_ASSERT_TRUE(parseTokenPoll(400, R"({"error":"access_denied"})").outcome == TokenPollOutcome::Denied);
    TEST_ASSERT_TRUE(parseTokenPoll(400, R"({"error":"Bad Request"})").outcome == TokenPollOutcome::Failed);
    TEST_ASSERT_TRUE(parseTokenPoll(200, R"({"token_type":"Bearer"})").outcome == TokenPollOutcome::Failed);
    TEST_ASSERT_TRUE(parseTokenPoll(-1, "").outcome == TokenPollOutcome::Failed);
}

static void test_encode_requests() {
    char body[64];

    TEST_ASSERT_TRUE(encodePairCodeRequest("esp32-a1b2c3d4e5f6", body, sizeof body));
    TEST_ASSERT_EQUAL_STRING(R"({"hardware_id":"esp32-a1b2c3d4e5f6"})", body);
    TEST_ASSERT_TRUE(encodeTokenRequest("uuid", body, sizeof body));
    TEST_ASSERT_EQUAL_STRING(R"({"device_code":"uuid"})", body);
}

static void test_encode_request_rejects_small_buffer() {
    char body[10];

    TEST_ASSERT_FALSE(encodeTokenRequest("uuid-that-does-not-fit", body, sizeof body));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_usage_reads_both_providers);
    RUN_TEST(test_parse_cursor_unknown_and_missing_windows_remain_distinct);
    RUN_TEST(test_parse_usage_null_percentage_is_unknown);
    RUN_TEST(test_parse_usage_without_data);
    RUN_TEST(test_parse_usage_ignores_context);
    RUN_TEST(test_parse_usage_clamps_percentage);
    RUN_TEST(test_parse_usage_missing_provider_block_is_disabled);
    RUN_TEST(test_parse_usage_lists_providers_in_screen_order);
    RUN_TEST(test_parse_usage_accepts_no_enabled_provider);
    RUN_TEST(test_parse_usage_rejects_invalid_payloads);
    RUN_TEST(test_parse_pair_code);
    RUN_TEST(test_parse_pair_code_defaults_interval);
    RUN_TEST(test_parse_pair_code_rejects_missing_or_oversized_fields);
    RUN_TEST(test_parse_token_poll_approved);
    RUN_TEST(test_parse_token_poll_errors);
    RUN_TEST(test_encode_requests);
    RUN_TEST(test_encode_request_rejects_small_buffer);
    return UNITY_END();
}
