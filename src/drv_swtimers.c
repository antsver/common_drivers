// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

//**************************************************************************************************
// Software timers driver
//**************************************************************************************************
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "drv_swtimers.h"

//==================================================================================================
//=========================================== MACROS ===============================================
//==================================================================================================

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//------------------------------------------------------------------------------
// Union to store either simple or full timer handler
//------------------------------------------------------------------------------
typedef union handler_u {
    swtimers_handler_cb_t full_cb;          // pointer to timer event handler callback - full, with parameters
    swtimers_handler_simple_cb_t simple_cb; // pointer to timer event handler callback - simple, without parameters
} handler_union_t;

//------------------------------------------------------------------------------
// Single timer structure
//------------------------------------------------------------------------------
typedef struct swtimers_timer_instance_s {

    // Settings
    handler_union_t handler;        // pointer to handler
    uint32_t        threshold;      // threshold for counter
    void*           arg_1_p;        // pointer to application data to be passed into handler (can be NULL)
    void*           arg_2_p;        // pointer to application data to be passed into handler (can be NULL)
    swtimers_mode_t mode;           // single shot or periodic mode
    bool            is_simple;      // 'true' - if simple_cb should be called

    // State
    bool            is_run;         // 'true' - if timer is started and counter increment is allowed
    bool            is_waiting;     // 'true' - if threshold is reached in ISR handler and the event handler should be called
    uint32_t        counter;        // counter of hardware timer interrupts

} swtimers_timer_instance_t;

//------------------------------------------------------------------------------
// Driver instance
//------------------------------------------------------------------------------
typedef struct swtimers_instance_s {
    const swtimers_hw_interface_t*      hw_p;              // pointer to hardware timer interface
    volatile swtimers_timer_instance_t* timers_table_p;    // pointer to array of timers
    uint32_t                            num;               // number of timers
} swtimers_instance_t;

//------------------------------------------------------------------------------
// Sanitizing
//------------------------------------------------------------------------------
static_assert(sizeof(swtimers_timer_instance_t) == sizeof(swtimers_timer_t), "Wrong structure size");
static_assert(sizeof(swtimers_instance_t) == sizeof(swtimers_t), "Wrong structure size");

//==================================================================================================
//================================ PRIVATE FUNCTIONS DECLARATIONS ==================================
//==================================================================================================

static void swtimers_start_hw_timer(const swtimers_t * inst_p);
static void swtimers_stop_hw_timer(const swtimers_t * inst_p);
static void swtimers_do_start(const swtimers_t * inst_p, uint32_t idx, uint32_t ms, swtimers_mode_t mode,
                              bool is_simple, swtimers_handler_cb_t handler_cb, swtimers_handler_simple_cb_t handler_simple_cb,
                              void * arg_1_p, void * arg_2_p);

//==================================================================================================
//==================================== PRIVATE STATIC DATA =========================================
//==================================================================================================

swtimers_instance_t swtimers_inst;

//==================================================================================================
//======================================== PUBLIC DATA =============================================
//==================================================================================================

//==================================================================================================
//================================ PUBLIC FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//------------------------------------------------------------------------------
// Init driver
//------------------------------------------------------------------------------
void swtimers_init(swtimers_t * inst_p, const swtimers_hw_interface_t * hw_interface_p, uint32_t num, volatile swtimers_timer_t * timers_table_p)
{
    assert((inst_p != NULL) && (hw_interface_p != NULL) && (num > 0) && (timers_table_p != NULL));
    assert((hw_interface_p->isr_disable_cb != NULL) && (hw_interface_p->isr_enable_cb != NULL) && (hw_interface_p->tick_ms != 0));
    assert(((hw_interface_p->hw_start_cb == NULL) && (hw_interface_p->hw_stop_cb == NULL) && (hw_interface_p->hw_is_started_cb == NULL)) ||
           ((hw_interface_p->hw_start_cb != NULL) && (hw_interface_p->hw_stop_cb != NULL) && (hw_interface_p->hw_is_started_cb != NULL)));

    swtimers_instance_t * swtimers_inst_p = (swtimers_instance_t*)inst_p;

    memset(inst_p, 0x00, sizeof(swtimers_t));

    swtimers_inst_p->hw_p = hw_interface_p;
    swtimers_inst_p->timers_table_p = (volatile swtimers_timer_instance_t*)timers_table_p;
    swtimers_inst_p->num = num;

    memset((swtimers_timer_instance_t*)swtimers_inst_p->timers_table_p, 0x00, num * sizeof(swtimers_timer_instance_t));

    swtimers_stop_all(inst_p);
}

//------------------------------------------------------------------------------
// Deinit driver
//------------------------------------------------------------------------------
void swtimers_deinit(swtimers_t * inst_p)
{
    assert(inst_p != NULL);
    swtimers_instance_t * swtimers_inst_p = (swtimers_instance_t*)inst_p;

    // If not initialized
    if (swtimers_inst_p->num == 0) {
        return;
    }

    swtimers_stop_all(inst_p);
    swtimers_stop_hw_timer(inst_p);

    memset((swtimers_timer_instance_t*)swtimers_inst_p->timers_table_p, 0x00, swtimers_inst_p->num * sizeof(swtimers_timer_instance_t));
    memset(swtimers_inst_p, 0x00, sizeof(swtimers_instance_t));
}

//------------------------------------------------------------------------------
// Start timer
//------------------------------------------------------------------------------
void swtimers_start(const swtimers_t * inst_p, uint32_t idx, uint32_t ms, swtimers_mode_t mode,
                    swtimers_handler_cb_t handler_cb, void * arg_1_p, void * arg_2_p)
{
    swtimers_do_start(inst_p, idx, ms, mode, false, handler_cb, NULL, arg_1_p, arg_2_p);
}

//------------------------------------------------------------------------------
// Start timer with simplified handler callback function
//------------------------------------------------------------------------------
void swtimers_start_simple(const swtimers_t * inst_p, uint32_t idx, uint32_t ms, swtimers_mode_t mode,
                           swtimers_handler_simple_cb_t handler_cb)
{
    swtimers_do_start(inst_p, idx, ms, mode, true, NULL, handler_cb, NULL, NULL);
}

//------------------------------------------------------------------------------
// Start single shot timer without handler
//------------------------------------------------------------------------------
void swtimers_start_no_handler(const swtimers_t * inst_p, uint32_t idx, uint32_t ms)
{
    swtimers_do_start(inst_p, idx, ms, SWTIMERS_MODE_SINGLE_FROM_LOOP, true, NULL, NULL, NULL, NULL);
}

//------------------------------------------------------------------------------
// Stop timer
//------------------------------------------------------------------------------
void swtimers_stop(const swtimers_t * inst_p, uint32_t idx)
{
    assert(inst_p != NULL);
    const swtimers_instance_t * swtimers_inst_p = (const swtimers_instance_t*)inst_p;
    assert(idx < swtimers_inst_p->num);
    volatile swtimers_timer_instance_t * swtimer_p = &(swtimers_inst_p->timers_table_p[idx]);
    const swtimers_hw_interface_t * hw_p = swtimers_inst_p->hw_p;

    // Critical section - stop timer
    hw_p->isr_disable_cb(hw_p->hw_timer_p);
    swtimer_p->is_run = false;
    swtimer_p->is_waiting = false;
    swtimer_p->counter = 0;
    hw_p->isr_enable_cb(hw_p->hw_timer_p);

    swtimers_stop_hw_timer(inst_p);
}

//------------------------------------------------------------------------------
// Stop all timers
//------------------------------------------------------------------------------
void swtimers_stop_all(const swtimers_t * inst_p)
{
    assert(inst_p != NULL);
    const swtimers_instance_t * swtimers_inst_p = (const swtimers_instance_t*)inst_p;
    assert(swtimers_inst_p->num != 0);

    for (size_t i = 0; i < swtimers_inst_p->num; ++i) {
        swtimers_stop(inst_p, i);
    }

    swtimers_stop_hw_timer(inst_p);
}

//------------------------------------------------------------------------------
// Check if timer is run and get time since the last start of the timer
//------------------------------------------------------------------------------
bool swtimers_is_run(const swtimers_t * inst_p, uint32_t idx, uint32_t * time_ms_out_p)
{
    assert(inst_p != NULL);
    const swtimers_instance_t * swtimers_inst_p = (const swtimers_instance_t*)inst_p;
    assert(idx < swtimers_inst_p->num);
    volatile swtimers_timer_instance_t * swtimer_p = &(swtimers_inst_p->timers_table_p[idx]);
    const swtimers_hw_interface_t * hw_p = swtimers_inst_p->hw_p;

    // Critical section - get state
    hw_p->isr_disable_cb(hw_p->hw_timer_p);
    bool is_run = swtimer_p->is_run;
    bool is_waiting = swtimer_p->is_waiting;
    uint32_t counter = swtimer_p->counter;
    hw_p->isr_enable_cb(hw_p->hw_timer_p);

    // If timer is run OR if timer is stopped but hash't been processed yet
    if (is_run || is_waiting) {
    	*time_ms_out_p = (counter * hw_p->tick_ms);
        return true;
    }
    else {
    	*time_ms_out_p = 0;
    	return false;
    }
}

//------------------------------------------------------------------------------
// Check all timers and call handlers if necessary
//------------------------------------------------------------------------------
void swtimers_task(const swtimers_t * inst_p)
{
    assert(inst_p != NULL);
    const swtimers_instance_t * swtimers_inst_p = (const swtimers_instance_t*)inst_p;
    assert(swtimers_inst_p->num != 0);
    volatile swtimers_timer_instance_t * swtimer_p;
    const swtimers_hw_interface_t * hw_p = swtimers_inst_p->hw_p;

    for (size_t i = 0; i < swtimers_inst_p->num; ++i) {
        swtimer_p = &(swtimers_inst_p->timers_table_p[i]);

        // Critical section - get state
        hw_p->isr_disable_cb(hw_p->hw_timer_p);
        bool is_waiting = swtimer_p->is_waiting;
        hw_p->isr_enable_cb(hw_p->hw_timer_p);

        if (is_waiting) {
            // Call handler
            if ((swtimer_p->is_simple == true) && (swtimer_p->handler.simple_cb != NULL)) {
                (swtimer_p->handler.simple_cb)();
            }
            else if ((swtimer_p->is_simple == false) && (swtimer_p->handler.full_cb != NULL)) {
                (swtimer_p->handler.full_cb)(i, swtimer_p->arg_1_p, swtimer_p->arg_2_p);
            }

            // Critical section - set state
            hw_p->isr_disable_cb(hw_p->hw_timer_p);
            swtimer_p->is_waiting = false;
            hw_p->isr_enable_cb(hw_p->hw_timer_p);
        }
    }

    swtimers_stop_hw_timer(inst_p);
}

//------------------------------------------------------------------------------
// ISR handler for hardware timer interrupt
//------------------------------------------------------------------------------
void swtimers_isr(const swtimers_t * inst_p)
{
    const swtimers_instance_t * swtimers_inst_p = (const swtimers_instance_t*)inst_p;

    for (size_t i = 0; i < swtimers_inst_p->num; ++i) {
    	volatile swtimers_timer_instance_t * swtimer_p = &(swtimers_inst_p->timers_table_p[i]);

        if (swtimer_p->is_run == false) {
            continue;
        }

        swtimer_p->counter++;

        if (swtimer_p->counter < swtimer_p->threshold) {
            continue;
        }

        if ((swtimer_p->mode == SWTIMERS_MODE_SINGLE_FROM_LOOP) || (swtimer_p->mode == SWTIMERS_MODE_SINGLE_FROM_ISR)) {
            // Stop single shot timer
            swtimer_p->is_run = false;
        }
        else {
            // Drop periodical counter
            swtimer_p->counter = 0;
        }

        // If handler exists - call handler from ISR context or set flag to call handler from application context
        if ((swtimer_p->handler.full_cb != NULL) || (swtimer_p->handler.simple_cb != NULL)) {
            if ((swtimer_p->mode == SWTIMERS_MODE_SINGLE_FROM_ISR) || (swtimer_p->mode == SWTIMERS_MODE_PERIODIC_FROM_ISR)) {
                if (swtimer_p->is_simple) {
                    (swtimer_p->handler.simple_cb)();
                }
                else {
                    (swtimer_p->handler.full_cb)(i, swtimer_p->arg_1_p, swtimer_p->arg_2_p);
                }
            }
            else {
                swtimer_p->is_waiting = true;
            }
        }
    }
}

//==================================================================================================
//================================ PRIVATE FUNCTIONS DEFINITIONS ===================================
//==================================================================================================

//------------------------------------------------------------------------------
// Start hardware timer if necessary
//------------------------------------------------------------------------------
static void swtimers_start_hw_timer(const swtimers_t * inst_p)
{
    assert(inst_p != NULL);
    const swtimers_instance_t * swtimers_inst_p = (const swtimers_instance_t*)inst_p;

    // If there is no interface to hardware timer
    if (swtimers_inst_p->hw_p->hw_is_started_cb == NULL) {
        return;
    }

    // If hardware timer is already started
    if (swtimers_inst_p->hw_p->hw_is_started_cb(swtimers_inst_p->hw_p->hw_timer_p)) {
        return;
    }

    swtimers_inst_p->hw_p->hw_start_cb(swtimers_inst_p->hw_p->hw_timer_p);
}

//------------------------------------------------------------------------------
// Stop hardware timer if necessary (if no timers are started)
//------------------------------------------------------------------------------
static void swtimers_stop_hw_timer(const swtimers_t * inst_p)
{
    assert(inst_p != NULL);
    const swtimers_instance_t * swtimers_inst_p = (const swtimers_instance_t*)inst_p;

    // If there is no interface to hardware timer
    if (swtimers_inst_p->hw_p->hw_is_started_cb == NULL) {
        return;
    }

    // If hardware timer is already stopped
    if (swtimers_inst_p->hw_p->hw_is_started_cb(swtimers_inst_p->hw_p->hw_timer_p) == false) {
        return;
    }

    // If at least one timer is still started
    for (size_t i = 0; i < swtimers_inst_p->num; ++i) {
        if (swtimers_inst_p->timers_table_p[i].is_run) {
            return;
        }
    }

    swtimers_inst_p->hw_p->hw_stop_cb(swtimers_inst_p->hw_p->hw_timer_p);
}

//------------------------------------------------------------------------------
// Start timer
//------------------------------------------------------------------------------
static void swtimers_do_start(const swtimers_t * inst_p, uint32_t idx, uint32_t ms, swtimers_mode_t mode,
                              bool is_simple, swtimers_handler_cb_t handler_cb, swtimers_handler_simple_cb_t handler_simple_cb,
                              void * arg_1_p, void * arg_2_p)
{
    assert(inst_p != NULL);
    const swtimers_instance_t * swtimers_inst_p = (const swtimers_instance_t*)inst_p;
    assert(idx < swtimers_inst_p->num);
    volatile swtimers_timer_instance_t * swtimer_p = &(swtimers_inst_p->timers_table_p[idx]);
    const swtimers_hw_interface_t * hw_p = swtimers_inst_p->hw_p;

    swtimers_stop(inst_p, idx);

    swtimer_p->is_simple = is_simple;
    if (is_simple) {
        swtimer_p->handler.simple_cb = handler_simple_cb;
    }
    else {
        swtimer_p->handler.full_cb = handler_cb;
    }
    swtimer_p->mode = mode;
    swtimer_p->arg_1_p = arg_1_p;
    swtimer_p->arg_2_p = arg_2_p;
    swtimer_p->threshold = ms / swtimers_inst_p->hw_p->tick_ms;

    // Critical section - start timer
    hw_p->isr_disable_cb(hw_p->hw_timer_p);
    swtimer_p->is_run = true;
    hw_p->isr_enable_cb(hw_p->hw_timer_p);

    swtimers_start_hw_timer(inst_p);
}



