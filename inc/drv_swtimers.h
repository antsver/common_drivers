//**************************************************************************************************
// Software timers driver
//**************************************************************************************************
// swtimers_task() should be called periodically from application loop to process timers' state
// swtimers_isr() should be called periodically from ISR context to provide timer ticks
//
// Driver uses a single hardware timer accessed over callback functions
//
// All functions are reenterable:
//  - driver doesn't use internal static data
//  - driver's instance and table of timers are supposed to be stored externally
//**************************************************************************************************

#ifndef DRV_SWTIMERS_H
#define DRV_SWTIMERS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//==================================================================================================
//=========================================== MACROS ===============================================
//==================================================================================================

//------------------------------------------------------------------------------
// Size of hidden structure swtimers_timer_t
//------------------------------------------------------------------------------
#define SWTIMERS_SINGLE_TIMER_INSTANCE_SIZE (24)

//------------------------------------------------------------------------------
// Size of hidden structure swtimers_t
//------------------------------------------------------------------------------
#define SWTIMERS_DRIVER_INSTANCE_SIZE (12)

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//------------------------------------------------------------------------------
// SW timer mode
//------------------------------------------------------------------------------
typedef enum swtimer_mode_e {
    SWTIMERS_MODE_SINGLE_FROM_LOOP,     // single shot timer,   call handler in application context from swtimers_task
    SWTIMERS_MODE_PERIODIC_FROM_LOOP,   // periodic timer,      call handler in application context from swtimers_task
    SWTIMERS_MODE_SINGLE_FROM_ISR,      // single shot timer,   call handler in ISR context from swtimers_isr
    SWTIMERS_MODE_PERIODIC_FROM_ISR,    // periodic timer,      call handler in ISR context from swtimers_isr
} swtimers_mode_t;

//------------------------------------------------------------------------------
// Callback - Timer handler (to be called to notify application about timer event)
// Handler shouldn't call init/deinit/task/isr functions
// Handler can call start/stop/is_run functions for any timer
//
// `timer_idx`  - index of SW timer
// `arg_1_p`    - pointer to application data, passed over swtimers_start function (can be NULL)
// `arg_2_p`    - pointer to application data, passed over swtimers_start function (can be NULL)
//------------------------------------------------------------------------------
typedef void (*swtimers_handler_cb_t)(uint32_t timer_idx, void * arg_1_p, void * arg_2_p);

//------------------------------------------------------------------------------
// Callback - Simple timer handler (to be called to notify application about timer event)
//------------------------------------------------------------------------------
typedef void (*swtimers_handler_simple_cb_t)(void);

//------------------------------------------------------------------------------
// Callback - Enable interrupt from hardware timer (to allow swtimers_isr handler calls)
// Callback - Disable interrupt from hardware timer (to allow swtimers_isr handler calls)
//
// `hw_timer_p` - pointer to hardware timer, passed over swtimers_hw_interface_t structure (can be NULL)
//------------------------------------------------------------------------------
typedef void (*swtimers_isr_ctrl_cb_t)(void * hw_timer_p);

//------------------------------------------------------------------------------
// Callback - Start hardware timer (to be called if at least one timer is started)
// Callback - Stop hardware timer (to be called if all timers are stopped, to reduce power consumption)
//
// `hw_timer_p` - pointer to hardware timer, passed over swtimers_hw_interface_t structure (can be NULL)
//------------------------------------------------------------------------------
typedef void (*swtimers_hw_ctrl_cb_t)(void * hw_timer_p);

//------------------------------------------------------------------------------
// Callback - Check if hardware timer is started
// Returns - 'true' if hardware timer is started (no matter enabled ISR or not), 'false' otherwise
//
// `hw_timer_p` - pointer to hardware timer, passed over swtimers_hw_interface_t structure (can be NULL)
//------------------------------------------------------------------------------
typedef bool (*swtimers_hw_is_started_cb_t)(void * hw_timer_p);

//------------------------------------------------------------------------------
// Interface to hardware timer
//------------------------------------------------------------------------------
typedef struct swtimers_hw_interface_s {
    void*                        hw_timer_p;        // Pointer to hardware timer to be passed into callbacks (can be NULL)
    swtimers_isr_ctrl_cb_t       isr_enable_cb;     // Enable interrupt from hardware timer
    swtimers_isr_ctrl_cb_t       isr_disable_cb;    // Disable interrupt from hardware timer
    swtimers_hw_ctrl_cb_t        hw_start_cb;       // Start hardware timer               (can be NULL if hw timer control isn't necessary)
    swtimers_hw_ctrl_cb_t        hw_stop_cb;        // Stop hardware timer                (can be NULL if hw timer control isn't necessary)
    swtimers_hw_is_started_cb_t  hw_is_started_cb;  // Check if hardware timer is started (can be NULL if hw timer control isn't necessary)
    uint32_t                     tick_ms;           // One tick of hardware timer in milliseconds (period of `swtimers_isr` calls)
} swtimers_hw_interface_t;

//------------------------------------------------------------------------------
// Single timer instance (structure is hidden in .c file)
//------------------------------------------------------------------------------
typedef struct swtimers_timer_s {
    uint8_t data[SWTIMERS_SINGLE_TIMER_INSTANCE_SIZE];
} swtimers_timer_t;

//------------------------------------------------------------------------------
// Driver instance (structure is hidden in .c file)
//------------------------------------------------------------------------------
typedef struct swtimers_s {
    uint8_t data[SWTIMERS_DRIVER_INSTANCE_SIZE];
} swtimers_t;

//==================================================================================================
//================================ PUBLIC FUNCTIONS DECLARATIONS ===================================
//==================================================================================================

//------------------------------------------------------------------------------
// Init software timers driver
//
// Initializes all fields of driver instance and of timers array
//
// `inst_p`         - pointer to driver instance, can be uninitialized
// `hw_interface_p` - pointer to driver's hardware interface, structure must be alive
//                    until deinitialization of the driver
// `num`            - number of timers (must be > 0)
// `timers_table_p` - pointer to volatile array of timers with size = num * sizeof(swtimers_timer_t) bytes
//------------------------------------------------------------------------------
void swtimers_init(swtimers_t * inst_p, const swtimers_hw_interface_t * hw_interface_p, uint32_t num,
                   volatile swtimers_timer_t * timers_table_p);

//------------------------------------------------------------------------------
// Deinit software timers driver
//
// Stops hardware timer
// Clears all fields of driver instance and timers array
//
// `inst_p` - pointer to driver instance, can be uninitialized
//------------------------------------------------------------------------------
void swtimers_deinit(swtimers_t * inst_p);

//------------------------------------------------------------------------------
// Start timer
//
// If timer is already started - stop it and restart
//
// `inst_p`     - pointer to initialized driver instance
// `idx`        - index of timer (must be 0 .. num-1)
// `ms`         - threshold for timer in milliseconds (can be 0)
// `mode`       - single or periodical run, call handler from application of from ISR
// `handler_cb` - pointer to handler callback (can be NULL)
// `arg_1_p`    - pointer to application data to be passed into handler callback (can be NULL)
// `arg_2_p`    - pointer to application data to be passed into handler callback (can be NULL)
//------------------------------------------------------------------------------
void swtimers_start(const swtimers_t * inst_p, uint32_t idx, uint32_t ms, swtimers_mode_t mode,
                    swtimers_handler_cb_t handler_cb, void * arg_1_p, void * arg_2_p);

//------------------------------------------------------------------------------
// Start timer with simplified handler callback function
//
// If timer is already started - stop it and restart
//
// `inst_p`     - pointer to initialized driver instance
// `idx`        - index of timer (must be 0 .. num-1)
// `ms`         - threshold for timer in milliseconds (can be 0)
// `mode`       - single or periodical run, call handler from application of from ISR
// `handler_cb` - pointer to simple handler callback (can be NULL)
//------------------------------------------------------------------------------
void swtimers_start_simple(const swtimers_t * inst_p, uint32_t idx, uint32_t ms, swtimers_mode_t mode, swtimers_handler_simple_cb_t handler_cb);

//------------------------------------------------------------------------------
// Start single shot timer without handler
//
// If timer is already started - stop it and restart
//
// `inst_p`     - pointer to initialized driver instance
// `idx`        - index of timer (must be 0 .. num-1)
// `ms`         - threshold for timer in milliseconds (can be 0)
//------------------------------------------------------------------------------
void swtimers_start_no_handler(const swtimers_t * inst_p, uint32_t idx, uint32_t ms);

//------------------------------------------------------------------------------
// Stop timer
//
// If timer is already stopped - do nothing
//
// `inst_p` - pointer to initialized driver instance
// `idx`    - index of timer (must be 0 .. num-1)
//------------------------------------------------------------------------------
void swtimers_stop(const swtimers_t * inst_p, uint32_t idx);

//------------------------------------------------------------------------------
// Stop all timers timer
//
// If timer is already stopped - do nothing
//
// `inst_p` - pointer to initialized driver instance
//------------------------------------------------------------------------------
void swtimers_stop_all(const swtimers_t * inst_p);

//------------------------------------------------------------------------------
// Check if timer is run and get time since the last start of the timer
//
// Single-shot timer is supposed to be run:
// - after start function call
// - if handler callback exists  - until exit from handler callback
// - if handler callback is NULL - until timer expiration in ISR
//
// Periodical timer is supposed to be run:
// - after start function call
// - until timer stop
//
// `inst_p`        - pointer to initialized driver instance
// `idx`           - index of timer (must be 0 .. num-1)
// `time_ms_out_p` - оut - milliseconds since start of the timer if timer is run, 0 otherwise (can be NULL)
//
// Returns - 'true' if timer is run, 'false' otherwise
//------------------------------------------------------------------------------
bool swtimers_is_run(const swtimers_t * inst_p, uint32_t idx, uint32_t * time_ms_out_p);

//------------------------------------------------------------------------------
// Check all SW timers and call handlers if necessary
//
// To be called periodically from main loop
//
// `inst_p` - pointer to initialized driver instance
//------------------------------------------------------------------------------
void swtimers_task(const swtimers_t * inst_p);

//------------------------------------------------------------------------------
// ISR handler for hardware timer interrupt
//
// To be called periodically from ISR with period specified in the hardware timer's interface
// Also can be called from application if software time measurement is used instead of hardware timer
//
// Increases counters of all active timers, compares them with thresholds, possibly calls handler callbacks
//
//
// `inst_p` - pointer to initialized driver instance
//------------------------------------------------------------------------------
void swtimers_isr(const swtimers_t * inst_p);

//==================================================================================================
//============================================ TESTS ===============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Run unit-tests
// Returns - number of failed test or 0 if all tests are successful
//-----------------------------------------------------------------------------
int32_t swtimers_tests(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DRV_SWTIMERS_H



