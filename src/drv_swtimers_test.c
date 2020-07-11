// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

//**************************************************************************************************
// TESTS for software timers driver, to be run on the target platform
//**************************************************************************************************

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "drv_swtimers.h"

//==================================================================================================
//============================================ TESTS ===============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Additional test functions
//-----------------------------------------------------------------------------
int32_t swtimers_test_all_timers_check(bool timers_state, uint32_t timers_cnt, bool hw_is_started, bool hw_isr_is_enabled, uint32_t handler_cnt);

static int32_t swtimers_test_cycle_1(uint32_t cycle);
static int32_t swtimers_test_cycle_2(uint32_t cycle);

static void swtimers_test_handler(uint32_t id, void * arg_1_p, void * arg_2_p);
static void swtimers_test_hw_isr_enable(void * hw_timer_p);
static void swtimers_test_hw_isr_disable(void * hw_timer_p);
static void swtimers_test_hw_start(void * hw_timer_p);
static void swtimers_test_hw_stop(void * hw_timer_p);
static bool swtimers_test_hw_is_started(void * hw_timer_p);

//-----------------------------------------------------------------------------
// Additional test data
//-----------------------------------------------------------------------------
#define SWTIMERS_TEST_TIMERS_NUM (10)

static uint8_t test_hw_timer_instance;
static uint8_t test_app_data;
bool test_hw_is_started = false;
bool test_hw_isr_is_enabled = true;

// Instances
static swtimers_t test_inst;
static swtimers_timer_t test_timers[SWTIMERS_TEST_TIMERS_NUM];

swtimers_hw_interface_t test_hw_interface = {
    .hw_timer_p = &test_hw_timer_instance,
    .isr_enable_cb = swtimers_test_hw_isr_enable,
    .isr_disable_cb = swtimers_test_hw_isr_disable,
    .hw_start_cb = swtimers_test_hw_start,
    .hw_stop_cb = swtimers_test_hw_stop,
    .hw_is_started_cb = swtimers_test_hw_is_started,
    .tick_ms = 1
};

// Counters
uint32_t test_handler_cnt = 0;
uint32_t test_hw_start_cnt = 0;
uint32_t test_hw_stop_cnt = 0;

//-----------------------------------------------------------------------------
// Run unit-tests
//-----------------------------------------------------------------------------
int32_t swtimers_tests(void)
{
    // Test cycle 1
    for (uint32_t i = 0; i < 10; i++) {

    	test_hw_is_started = false;
        test_hw_isr_is_enabled = true;
        test_handler_cnt = 0;
        test_hw_start_cnt = 0;
        test_hw_stop_cnt = 0;

    	int32_t res = swtimers_test_cycle_1(1000 + 100 * i); // res 1000 - 1999
        if (res != 0) {
            return res;
        }
    }

    // Test cycle 2
    for (uint32_t i = 0; i < 10; i++) {

    	test_hw_is_started = false;
        test_hw_isr_is_enabled = true;
        test_handler_cnt = 0;
        test_hw_start_cnt = 0;
        test_hw_stop_cnt = 0;

    	int32_t res = swtimers_test_cycle_2(2000 + 100 * i); // res 2000 - 2999
        if (res != 0) {
            return res;
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------
// TEST CHECK
//-----------------------------------------------------------------------------
int32_t swtimers_test_all_timers_check(bool timers_state, uint32_t timers_cnt, bool hw_is_started, bool hw_isr_is_enabled, uint32_t handler_cnt)
{

    for (uint32_t i = 0; i < SWTIMERS_TEST_TIMERS_NUM; i++) {
        uint32_t ms;
        if (swtimers_is_run(&test_inst, i, &ms) != timers_state) {
            return 1;
        }
        if (ms != timers_cnt) {
            return 2;
        }
    }

    // CHECK - hw timer disabled
    if (test_hw_is_started != hw_is_started) {
        return 3;
    }
    if (test_hw_isr_is_enabled != hw_isr_is_enabled) {
        return 4;
    }

    // CHECK - handler calls 0
    if (test_handler_cnt != handler_cnt) {
        return 5;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Test cycle 1
//-----------------------------------------------------------------------------
static int32_t swtimers_test_cycle_1(uint32_t cycle)
{
    int32_t res;

    // TEST - init driver
    swtimers_init(&test_inst, &test_hw_interface, SWTIMERS_TEST_TIMERS_NUM, test_timers);
    // CHECK
    res = swtimers_test_all_timers_check(false, 0, false, true, 0);
    if (res != 0) {
        return cycle + 10 + res;
    }

    // TEST - stop all timers
    for (uint32_t i = 0; i < SWTIMERS_TEST_TIMERS_NUM; i++) {
        swtimers_stop(&test_inst, i);
    }
    // CHECK
    res = swtimers_test_all_timers_check(false, 0, false, true, 0);
    if (res != 0) {
        return cycle + 20 + res;
    }

    // TEST - start all timers: 2 ms, single, from loop
    for (uint32_t i = 0; i < SWTIMERS_TEST_TIMERS_NUM; i++) {
        swtimers_start(&test_inst, i, 2, SWTIMERS_MODE_SINGLE_FROM_LOOP, swtimers_test_handler, &test_app_data, &test_app_data);
    }
    // CHECK
    res = swtimers_test_all_timers_check(true, 0, true, true, 0);
    if (res != 0) {
        return cycle + 30 + res;
    }

    // TEST - task
    swtimers_task(&test_inst);
    // CHECK
    res = swtimers_test_all_timers_check(true, 0, true, true, 0);
    if (res != 0) {
        return cycle + 40 + res;
    }

    // TEST - ISR = 1
    swtimers_isr(&test_inst);
    // CHECK
    res = swtimers_test_all_timers_check(true, 1, true, true, 0);
    if (res != 0) {
        return cycle + 50 + res;
    }

    // TEST - task
    swtimers_task(&test_inst);
    // CHECK
    res = swtimers_test_all_timers_check(true, 1, true, true, 0);
    if (res != 0) {
        return cycle + 60 + res;
    }

    // TEST - ISR = 2
    swtimers_isr(&test_inst);
    // CHECK (timer is expited but assumed to be run until processing from task)
    res = swtimers_test_all_timers_check(true, 2, true, true, 0);
    if (res != 0) {
        return cycle + 70 + res;
    }

    // TEST - task
    swtimers_task(&test_inst);
    // CHECK
    res = swtimers_test_all_timers_check(false, 0, false, true, SWTIMERS_TEST_TIMERS_NUM);
    if (res != 0) {
        return cycle + 80 + res;
    }

    // TEST - ISR = 3
    swtimers_isr(&test_inst);
    // CHECK
    res = swtimers_test_all_timers_check(false, 0, false, true, SWTIMERS_TEST_TIMERS_NUM);
    if (res != 0) {
        return cycle + 90 + res;
    }

    // TEST - deinit
    swtimers_deinit(&test_inst);

    // TEST - deinit without init
    swtimers_deinit(&test_inst);

    return 0;
}

//-----------------------------------------------------------------------------
// Test cycle 2
//-----------------------------------------------------------------------------
static int32_t swtimers_test_cycle_2(uint32_t cycle)
{
    int32_t res;

    // TEST - init driver and start all timers: 2 ms, periodic, from isr
    swtimers_init(&test_inst, &test_hw_interface, SWTIMERS_TEST_TIMERS_NUM, test_timers);
    // CHECK
    res = swtimers_test_all_timers_check(false, 0, false, true, 0);
    if (res != 0) {
        return cycle + 10 + res;
    }


    // TEST - start all timers: 2 ms, periodic, from isr
    for (uint32_t i = 0; i < SWTIMERS_TEST_TIMERS_NUM; i++) {
        swtimers_start(&test_inst, i, 2, SWTIMERS_MODE_PERIODIC_FROM_ISR, swtimers_test_handler, &test_app_data, &test_app_data);
    }
    // CHECK
    res = swtimers_test_all_timers_check(true, 0, true, true, 0);
    if (res != 0) {
        return cycle + 20 + res;
    }


    // TEST - ISR = 1
    swtimers_isr(&test_inst);
    // CHECK
    res = swtimers_test_all_timers_check(true, 1, true, true, 0);
    if (res != 0) {
        return cycle + 30 + res;
    }


    // TEST - ISR = 2
    swtimers_isr(&test_inst);
    // CHECK
    res = swtimers_test_all_timers_check(true, 0, true, true, SWTIMERS_TEST_TIMERS_NUM);
    if (res != 0) {
        return cycle + 40 + res;
    }


    // TEST - ISR = 3
    swtimers_isr(&test_inst);
    // CHECK
    res = swtimers_test_all_timers_check(true, 1, true, true, SWTIMERS_TEST_TIMERS_NUM);
    if (res != 0) {
        return cycle + 50 + res;
    }


    // TEST - ISR = 4
    swtimers_isr(&test_inst);
    // CHECK
    res = swtimers_test_all_timers_check(true, 0, true, true, 2 * SWTIMERS_TEST_TIMERS_NUM);
    if (res != 0) {
        return cycle + 60 + res;
    }


    // TEST - stop all timers
    for (uint32_t i = 0; i < SWTIMERS_TEST_TIMERS_NUM; i++) {
        swtimers_stop(&test_inst, i);
    }
    // CHECK
    res = swtimers_test_all_timers_check(false, 0, false, true, 2 * SWTIMERS_TEST_TIMERS_NUM);
    if (res != 0) {
        return cycle + 70 + res;
    }


    // TEST - deinit
    swtimers_deinit(&test_inst);

    return 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void swtimers_test_handler(uint32_t id, void * arg_1_p, void * arg_2_p)
{
    (void)id;
    (void)arg_1_p;
    (void)arg_2_p;
    assert((arg_1_p == &test_app_data) && (arg_2_p == &test_app_data) && (id < SWTIMERS_TEST_TIMERS_NUM));

    test_handler_cnt++;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void swtimers_test_hw_isr_enable(void * hw_timer_p)
{
    (void)hw_timer_p;
    assert(hw_timer_p == &test_hw_timer_instance);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void swtimers_test_hw_isr_disable(void * hw_timer_p)
{
    (void)hw_timer_p;
    assert(hw_timer_p == &test_hw_timer_instance);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void swtimers_test_hw_start(void * hw_timer_p)
{
    (void)hw_timer_p;
    assert(hw_timer_p == &test_hw_timer_instance);

    test_hw_is_started = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static void swtimers_test_hw_stop(void * hw_timer_p)
{
    (void)hw_timer_p;
    assert(hw_timer_p == &test_hw_timer_instance);

    test_hw_is_started = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static bool swtimers_test_hw_is_started(void * hw_timer_p)
{
    (void)hw_timer_p;
    assert(hw_timer_p == &test_hw_timer_instance);

    return test_hw_is_started;
}


