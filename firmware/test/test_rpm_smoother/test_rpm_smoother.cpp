#include <unity.h>

#include "core/rpm_smoother.h"

void test_step_moves_one_quarter_of_gap() {
    RpmSmoother smoother;
    smoother.setTarget(12000);
    TEST_ASSERT_EQUAL_UINT16(3000, smoother.step());
}

void test_snap_when_gap_is_less_than_twenty() {
    RpmSmoother smoother;
    smoother.setTarget(30);
    smoother.step();
    smoother.setTarget(20);
    TEST_ASSERT_EQUAL_UINT16(20, smoother.step());
}

void test_set_target_clamps_to_maximum() {
    RpmSmoother smoother;
    smoother.setTarget(16000);
    TEST_ASSERT_EQUAL_UINT16(12000, smoother.target());
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_step_moves_one_quarter_of_gap);
    RUN_TEST(test_snap_when_gap_is_less_than_twenty);
    RUN_TEST(test_set_target_clamps_to_maximum);
    return UNITY_END();
}
