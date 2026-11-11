#include <unity.h>

#include "core/window_selection.h"

void setUp() {}
void tearDown() {}

static UsageWindow presentWindow() {
    UsageWindow window;
    window.present = true;
    window.known = true;
    window.usedPercentage = 10.0f;
    return window;
}

static ProviderUsage weeklyOnly(ProviderId id) {
    ProviderUsage entry;
    entry.id = id;
    entry.view.hasData = true;
    entry.view.sevenDay = presentWindow();
    return entry;
}

static void test_argon_keeps_the_five_hour_tab_selectable() {
    const ProviderUsage argon = weeklyOnly(ProviderId::AntigravityArgon);

    TEST_ASSERT_FALSE(effectiveSecondary(argon, false));
    TEST_ASSERT_TRUE(effectiveSecondary(argon, true));
}

static void test_other_weekly_only_providers_still_fall_back_to_seven_days() {
    TEST_ASSERT_TRUE(effectiveSecondary(weeklyOnly(ProviderId::Claude), false));
    TEST_ASSERT_TRUE(effectiveSecondary(weeklyOnly(ProviderId::AntigravityGemini), false));
}

static void test_provider_with_both_windows_follows_the_choice() {
    ProviderUsage claude = weeklyOnly(ProviderId::AntigravityClaude);
    claude.view.fiveHour = presentWindow();

    TEST_ASSERT_FALSE(effectiveSecondary(claude, false));
    TEST_ASSERT_TRUE(effectiveSecondary(claude, true));
}

static void test_cursor_never_falls_back() {
    const ProviderUsage cursor = weeklyOnly(ProviderId::Cursor);

    TEST_ASSERT_FALSE(effectiveSecondary(cursor, false));
    TEST_ASSERT_FALSE(reportsOnlyWeekly(cursor));
}

static void test_argon_still_reports_only_weekly_for_the_overview() {
    TEST_ASSERT_TRUE(reportsOnlyWeekly(weeklyOnly(ProviderId::AntigravityArgon)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_argon_keeps_the_five_hour_tab_selectable);
    RUN_TEST(test_other_weekly_only_providers_still_fall_back_to_seven_days);
    RUN_TEST(test_provider_with_both_windows_follows_the_choice);
    RUN_TEST(test_cursor_never_falls_back);
    RUN_TEST(test_argon_still_reports_only_weekly_for_the_overview);
    return UNITY_END();
}
