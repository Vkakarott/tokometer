#include <unity.h>

#include "core/text_format.h"

void setUp() {}
void tearDown() {}

static UsageWindow knownWindow(float percentage, int64_t resetsAt = 0) {
    UsageWindow window;
    window.present = true;
    window.known = true;
    window.usedPercentage = percentage;
    window.resetsAt = resetsAt;
    return window;
}

static void test_primary_value_rounds_percentage() {
    char text[16];

    formatPrimaryValue(knownWindow(61.6f), text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("62%", text);
    formatPrimaryValue(knownWindow(100.0f), text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("100%", text);
}

static void test_primary_value_never_shows_unknown_number() {
    char text[16];
    UsageWindow reset = knownWindow(80.0f);
    reset.known = false;

    formatPrimaryValue(reset, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("--%", text);
    formatPrimaryValue(UsageWindow{}, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("--%", text);
}

static void test_fill_fraction() {
    UsageWindow reset = knownWindow(80.0f);
    reset.known = false;

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, usageFillFraction(knownWindow(25.0f)));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, usageFillFraction(reset));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, usageFillFraction(UsageWindow{}));
}

static void test_reset_in_formats_durations() {
    char text[16];
    const int64_t now = 1738400000;

    formatResetIn(knownWindow(1, now + 2 * 3600 + 14 * 60), now, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("2h14", text);
    formatResetIn(knownWindow(1, now + 45 * 60), now, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("45min", text);
    formatResetIn(knownWindow(1, now + 6 * 86400 + 12 * 3600), now, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("6d12h", text);
    formatResetIn(knownWindow(1, now + 30), now, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("<1min", text);
    formatResetIn(knownWindow(1, now - 30), now, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("<1min", text);
}

static void test_reset_in_is_empty_without_clock_or_value() {
    char text[16] = "x";
    UsageWindow reset = knownWindow(1, 1738400000);
    reset.known = false;

    formatResetIn(knownWindow(1, 1738400000), 0, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("", text);
    formatResetIn(reset, 1738300000, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("", text);
}

static void test_age() {
    char text[16];

    formatAge(30, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("agora", text);
    formatAge(180, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("3min", text);
    formatAge(7200, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("2h", text);
    formatAge(172800, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("2d", text);
}

static void test_user_code_gets_display_hyphen() {
    char text[16];

    formatUserCode("K7QM3F9A", text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("K7QM-3F9A", text);
    formatUserCode("ABC", text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("ABC", text);
}

static void test_countdown() {
    char text[16];

    formatCountdown(299, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("4:59", text);
    formatCountdown(0, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("0:00", text);
}

static void test_status_says_no_data_for_the_absent_five_hour_window_of_argon() {
    UsageView argon;
    argon.hasData = true;
    argon.sevenDay = knownWindow(4.0f, 1000);
    char text[24];

    formatWindowStatus(argon, argon.fiveHour, 500, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("Sem dados", text);
    formatWindowStatus(argon, argon.sevenDay, 500, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("8min", text);
}

static void test_status_without_provider_data_says_no_data() {
    UsageView empty;
    char text[24];

    formatWindowStatus(empty, knownWindow(10.0f, 1000), 500, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("Sem dados", text);
}

static void test_status_reports_stale_waiting_and_missing_clock() {
    UsageView view;
    view.hasData = true;
    UsageWindow reset = knownWindow(50.0f, 1000);
    char text[24];

    view.stale = true;
    view.ageSeconds = 180;
    formatWindowStatus(view, reset, 500, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("! 3min", text);

    view.stale = false;
    reset.known = false;
    formatWindowStatus(view, reset, 500, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("Aguardando", text);

    formatWindowStatus(view, knownWindow(50.0f, 1000), 0, text, sizeof text);
    TEST_ASSERT_EQUAL_STRING("Sem horario", text);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_primary_value_rounds_percentage);
    RUN_TEST(test_primary_value_never_shows_unknown_number);
    RUN_TEST(test_fill_fraction);
    RUN_TEST(test_reset_in_formats_durations);
    RUN_TEST(test_reset_in_is_empty_without_clock_or_value);
    RUN_TEST(test_age);
    RUN_TEST(test_user_code_gets_display_hyphen);
    RUN_TEST(test_countdown);
    RUN_TEST(test_status_says_no_data_for_the_absent_five_hour_window_of_argon);
    RUN_TEST(test_status_without_provider_data_says_no_data);
    RUN_TEST(test_status_reports_stale_waiting_and_missing_clock);
    return UNITY_END();
}
