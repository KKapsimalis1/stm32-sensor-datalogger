#include "unity.h"
#include "watchdog_core.h"

static WdgCore w;

void setUp(void)
{
    WdgCore_Init(&w);
}

void tearDown(void) {}

void test_no_registered_tasks_means_nothing_is_stalled(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, WdgCore_StalledMask(&w, 1000000));
}

void test_task_is_healthy_within_its_limit(void)
{
    WdgCore_Register(&w, 0, 1000, 500);
    TEST_ASSERT_EQUAL_UINT32(0, WdgCore_StalledMask(&w, 1000));
    TEST_ASSERT_EQUAL_UINT32(0, WdgCore_StalledMask(&w, 1500));   /* exactly at the limit */
}

void test_task_is_stalled_after_its_limit(void)
{
    WdgCore_Register(&w, 0, 1000, 500);
    TEST_ASSERT_EQUAL_UINT32(0x1, WdgCore_StalledMask(&w, 1501));
}

void test_checkin_restarts_the_silence_timer(void)
{
    WdgCore_Register(&w, 2, 0, 500);
    WdgCore_Checkin(&w, 2, 400);
    TEST_ASSERT_EQUAL_UINT32(0, WdgCore_StalledMask(&w, 800));
    TEST_ASSERT_EQUAL_UINT32(0x4, WdgCore_StalledMask(&w, 901));
}

void test_each_task_is_reported_in_its_own_bit(void)
{
    WdgCore_Register(&w, 0, 0, 100);
    WdgCore_Register(&w, 1, 0, 100);
    WdgCore_Register(&w, 3, 0, 100);

    WdgCore_Checkin(&w, 1, 150);                                  /* only task 1 is alive */
    TEST_ASSERT_EQUAL_UINT32((1u << 0) | (1u << 3), WdgCore_StalledMask(&w, 200));
}

void test_tasks_have_independent_limits(void)
{
    WdgCore_Register(&w, 0, 0, 100);
    WdgCore_Register(&w, 1, 0, 5000);
    TEST_ASSERT_EQUAL_UINT32(0x1, WdgCore_StalledMask(&w, 1000));
}

void test_set_limit_changes_the_allowed_silence(void)
{
    WdgCore_Register(&w, 0, 0, 1000);
    WdgCore_SetLimit(&w, 0, 60000);          /* for example the sampling period was raised */
    TEST_ASSERT_EQUAL_UINT32(0, WdgCore_StalledMask(&w, 30000));
    WdgCore_SetLimit(&w, 0, 1000);
    TEST_ASSERT_EQUAL_UINT32(0x1, WdgCore_StalledMask(&w, 30000));
}

void test_checkin_of_an_unregistered_task_is_ignored(void)
{
    WdgCore_Checkin(&w, 4, 123);
    TEST_ASSERT_EQUAL_UINT32(0, w.registered_mask);
    TEST_ASSERT_EQUAL_UINT32(0, WdgCore_StalledMask(&w, 1000000));
}

void test_out_of_range_ids_are_ignored(void)
{
    WdgCore_Register(&w, WDGCORE_MAX_TASKS, 0, 100);
    WdgCore_Register(&w, 1000, 0, 100);
    WdgCore_Checkin(&w, WDGCORE_MAX_TASKS, 5);
    WdgCore_SetLimit(&w, WDGCORE_MAX_TASKS, 5);
    TEST_ASSERT_EQUAL_UINT32(0, w.registered_mask);
    TEST_ASSERT_EQUAL_UINT32(0, WdgCore_StalledMask(&w, 1000000));
}

void test_time_counter_wraparound_is_handled(void)
{
    /* The task checks in 100 ms before the counter wraps. */
    WdgCore_Register(&w, 0, 0xFFFFFF9Cu, 500);
    TEST_ASSERT_EQUAL_UINT32(0, WdgCore_StalledMask(&w, 0x00000100u));   /* 356 ms later */
    TEST_ASSERT_EQUAL_UINT32(0x1, WdgCore_StalledMask(&w, 0x00000200u)); /* 612 ms later */
}

void test_registering_again_resets_the_task(void)
{
    WdgCore_Register(&w, 0, 0, 100);
    TEST_ASSERT_EQUAL_UINT32(0x1, WdgCore_StalledMask(&w, 1000));
    WdgCore_Register(&w, 0, 1000, 100);
    TEST_ASSERT_EQUAL_UINT32(0, WdgCore_StalledMask(&w, 1050));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_registered_tasks_means_nothing_is_stalled);
    RUN_TEST(test_task_is_healthy_within_its_limit);
    RUN_TEST(test_task_is_stalled_after_its_limit);
    RUN_TEST(test_checkin_restarts_the_silence_timer);
    RUN_TEST(test_each_task_is_reported_in_its_own_bit);
    RUN_TEST(test_tasks_have_independent_limits);
    RUN_TEST(test_set_limit_changes_the_allowed_silence);
    RUN_TEST(test_checkin_of_an_unregistered_task_is_ignored);
    RUN_TEST(test_out_of_range_ids_are_ignored);
    RUN_TEST(test_time_counter_wraparound_is_handled);
    RUN_TEST(test_registering_again_resets_the_task);
    return UNITY_END();
}
