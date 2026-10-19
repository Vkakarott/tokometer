#include <unity.h>

#include <initializer_list>

#include "core/usage_layout.h"

void setUp() {}
void tearDown() {}

static ProvidersUsage usageWith(std::initializer_list<ProviderId> ids) {
    ProvidersUsage usage;
    for (ProviderId id : ids) usage.items[usage.count++].id = id;
    return usage;
}

static void assertLayout(UsageLayout expected, UsageLayout actual) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>(actual));
}

static void test_cycles_through_every_layout_with_all_providers() {
    const auto usage = usageWith({ProviderId::Codex, ProviderId::Claude, ProviderId::Cursor});

    assertLayout(UsageLayout::CodexDashboard, nextLayout(UsageLayout::Overview, usage));
    assertLayout(UsageLayout::ClaudeDashboard, nextLayout(UsageLayout::CodexDashboard, usage));
    assertLayout(UsageLayout::CursorDashboard, nextLayout(UsageLayout::ClaudeDashboard, usage));
    assertLayout(UsageLayout::Overview, nextLayout(UsageLayout::CursorDashboard, usage));
}

static void test_skips_the_dashboard_of_a_disabled_provider() {
    const auto usage = usageWith({ProviderId::Codex, ProviderId::Claude});

    assertLayout(UsageLayout::Overview, nextLayout(UsageLayout::ClaudeDashboard, usage));
    TEST_ASSERT_FALSE(isLayoutAvailable(UsageLayout::CursorDashboard, usage));
}

static void test_single_provider_has_only_its_dashboard() {
    const auto usage = usageWith({ProviderId::Claude});

    TEST_ASSERT_FALSE(isLayoutAvailable(UsageLayout::Overview, usage));
    assertLayout(UsageLayout::ClaudeDashboard, resolveLayout(UsageLayout::Overview, usage));
    assertLayout(UsageLayout::ClaudeDashboard, nextLayout(UsageLayout::ClaudeDashboard, usage));
}

static void test_keeps_the_saved_layout_while_it_is_available() {
    const auto usage = usageWith({ProviderId::Codex, ProviderId::Cursor});

    assertLayout(UsageLayout::CursorDashboard, resolveLayout(UsageLayout::CursorDashboard, usage));
}

static void test_falls_forward_when_the_saved_dashboard_disappears() {
    const auto usage = usageWith({ProviderId::Codex, ProviderId::Claude});

    assertLayout(UsageLayout::Overview, resolveLayout(UsageLayout::CursorDashboard, usage));
}

static void test_no_provider_resolves_to_the_overview() {
    const ProvidersUsage usage;

    assertLayout(UsageLayout::Overview, resolveLayout(UsageLayout::ClaudeDashboard, usage));
    assertLayout(UsageLayout::Overview, nextLayout(UsageLayout::Overview, usage));
}

static void test_rejects_an_out_of_range_saved_index() {
    const auto usage = usageWith({ProviderId::Codex, ProviderId::Claude});

    assertLayout(UsageLayout::Overview, resolveLayout(static_cast<UsageLayout>(9), usage));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cycles_through_every_layout_with_all_providers);
    RUN_TEST(test_skips_the_dashboard_of_a_disabled_provider);
    RUN_TEST(test_single_provider_has_only_its_dashboard);
    RUN_TEST(test_keeps_the_saved_layout_while_it_is_available);
    RUN_TEST(test_falls_forward_when_the_saved_dashboard_disappears);
    RUN_TEST(test_no_provider_resolves_to_the_overview);
    RUN_TEST(test_rejects_an_out_of_range_saved_index);
    return UNITY_END();
}
