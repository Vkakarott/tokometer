#include <unity.h>

void test_core_smoke() {
    TEST_ASSERT_TRUE(true);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_core_smoke);
    return UNITY_END();
}
