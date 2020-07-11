//**************************************************************************************************
// LED control driver
//**************************************************************************************************
// Depends on drv_swtimers:
//  - swtimers driver should be initialized before any usage of this driver
//  - swtimers_task() should be called periodically from application loop
//  - swtimers_isr() should be called periodically from ISR context
//  - each LED occupies one software timer
//
// Driver uses GPIO-ouitputs accessed over callback functions
//
// Each LED state can be updated:
//  - manually in on-off mode (by calling leds_on() or leds_off() or leds_toggle() function)
//  - automatically in blinking mode (swtimers_task() call internal handler by pointer)
//
// All functions are reenterable:
//  - driver doesn't use internal static data
//  - driver's instance and table of LEDs are supposed to be stored externally
//
//**************************************************************************************************
// Examples
//**************************************************************************************************
// Example: meander
//      _________           _________           _________           _________
//  ___|         |_________|         |_________|         |_________|         |________ _ _
//     ^ "pulse"   "pause"   "pulse"   "pause"   "pulse"   "pause"   "pulse"
//     |
//     |
//     |- call leds_meander()
//
//
//
// Example: simple blink, series = 2
//
//                      |<----------------- period_ms ----------------->|
//  ___                  _________           _________                   _________
//     |________________|         |_________|         |_________________|         |___ _ _
//     ^    "delay"       "pulse"   "pause"   "pulse"       "wait"        "pulse"
//     |
//     |
//     |- call leds_blink()
//
//
//
// Example: simple blink, series = 2, with leds_switch_off() call
//
//                      |<----------------- period_ms ----------------->|
//  ___                  _________           ____  _ _                   _________
//     |________________|         |_________|    |____|_________________|         |___ _ _
//     ^    "delay"       "pulse"   "pause"   "pulse"       "wait"        "pulse"
//     |                                         ^
//     |                                         |
//     |- call leds_blink()                      |- call leds_switch_off() to force LED switching without stopping of blinking
//
//
//
// Example: extended blink, series = 2, is_inverted = false
//
//                      |<----------------- period_ms ----------------->|
//  ___                  _________           _________                   _________           _________
//     |________________|         |_________|         |_________________|         |_________|         |______________ _ _
//     ^    "delay"       "pulse"   "pause"   "pulse"       "wait"        "pulse"   "pause"   "pulse"       "wait"
//     |
//     |
//     |- call leds_blink_ext()
//
//
//
// Example: extended blink, series = 2, is_inverted = true
//
//                      |<----------------- period_ms ----------------->|
//      ________________           _________           _________________           _________           ______________ _ _
//  ___|                |_________|         |_________|                 |_________|         |_________|
//     ^    "delay"       "pulse"   "pause"   "pulse"       "wait"        "pulse"   "pause"   "pulse"       "wait"
//     |
//     |
//     |- call leds_blink_ext()
//
//**************************************************************************************************
#ifndef DRV_LEDS_H
#define DRV_LEDS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "drv_swtimers.h"

#ifdef __cplusplus
extern "C" {
#endif

//==================================================================================================
//=========================================== MACROS ===============================================
//==================================================================================================

//------------------------------------------------------------------------------
// Size of hidden structure leds_led_t
//------------------------------------------------------------------------------
#define LEDS_SINGLE_LED_INSTANCE_SIZE (28)

//------------------------------------------------------------------------------
// Size of hidden structure leds_t
//------------------------------------------------------------------------------
#define LEDS_DRIVER_INSTANCE_SIZE (16)

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//------------------------------------------------------------------------------
// Callback - Set hardware GPIO state
//
// `hw_gpio_p` - pointer to hardware GPIO driver, passed over leds_hw_interface_t structure
// `pin_idx`   - index of hardware GPIO pin, passed over leds_set_pin function
// `pin_state` - pin state, '0' - logical zero output, otherwise - logical one output
//------------------------------------------------------------------------------
typedef void (*leds_gpio_write_cb_t)(void * hw_gpio_p, uint32_t pin_idx, uint8_t pin_state);

//------------------------------------------------------------------------------
// Callback - Toggle hardware GPIO state
//
// `hw_gpio_p` - pointer to hardware GPIO driver, passed over leds_hw_interface_t structure
// `pin_idx`   - index of hardware GPIO pin, passed over leds_set_pin function
//------------------------------------------------------------------------------
typedef void (*leds_gpio_toggle_cb_t)(void * hw_gpio_p, uint32_t pin_idx);

//------------------------------------------------------------------------------
// Interface to hardware GPIO
//------------------------------------------------------------------------------
typedef struct leds_hw_interface_s {
    void*                        hw_gpio_p;     // Pointer to hardware GPIO driver to be passed into callbacks (can be NULL)
    leds_gpio_write_cb_t         gpio_write;    // Set hardware GPIO state
    leds_gpio_toggle_cb_t        gpio_toggle;   // Toggle hardware GPIO state
} leds_hw_interface_t;

//------------------------------------------------------------------------------
// Single LED instance (structure is hidden in .c file)
//------------------------------------------------------------------------------
typedef struct leds_led_s {
    uint8_t data[LEDS_SINGLE_LED_INSTANCE_SIZE];
} leds_led_t;

//------------------------------------------------------------------------------
// Driver instance (structure is hidden in .c file)
//------------------------------------------------------------------------------
typedef struct leds_s {
    uint8_t data[LEDS_DRIVER_INSTANCE_SIZE];
} leds_t;

//==================================================================================================
//================================ PUBLIC FUNCTIONS DECLARATIONS ===================================
//==================================================================================================

//------------------------------------------------------------------------------
// Init LED control driver

// `inst_p`         - pointer to driver instance, can be uninitialized
// `hw_interface_p` - pointer to driver's hardware interface (structure must be alive until deinitialization of the driver)
// `num`            - number of LEDs (must be > 0)
// `leds_table_p`   - pointer to volatile array of timers with size = num * sizeof(leds_led_t) bytes
// `swtimers_p`     - pointer to initialized software timers driver instance
//------------------------------------------------------------------------------
void leds_init(leds_t * inst_p, const leds_hw_interface_t * hw_interface_p, uint32_t num, leds_led_t * leds_table_p, const swtimers_t * swtimers_p);

//------------------------------------------------------------------------------
// Deinit LED control driver
// Turns OFF all LEDs
//
// `inst_p` - pointer to initialized driver instance
//------------------------------------------------------------------------------
void leds_deinit(leds_t * inst_p);

//------------------------------------------------------------------------------
// Match LED and physical GPIO pin
//
// `inst_p`         - pointer to initialized driver instance
// `idx`            - LED number (must be 0 .. num-1)
// `pin`            - index of GPIO pin connected to the LED
// `timer_id`       - id of software timer to measure blinking time
// `is_active_high` - 'true' if LED is active-high, 'false' if LED is active-low
//------------------------------------------------------------------------------
void leds_set_pin(leds_t * inst_p, uint32_t idx, uint32_t pin_idx, uint32_t timer_idx, bool is_active_high);

//------------------------------------------------------------------------------
// ON / OFF / Toggle LED and stay in current blinking mode
//
// `inst_p` - pointer to initialized driver instance
// `idx`    - LED number (must be 0 .. num-1)
//------------------------------------------------------------------------------
void leds_switch_on(const leds_t * inst_p, uint32_t idx);
void leds_switch_off(const leds_t * inst_p, uint32_t idx);
void leds_switch_toggle(const leds_t * inst_p, uint32_t idx);

//------------------------------------------------------------------------------
// ON / OFF / Toggle LED and leave current blinking mode
//
// `inst_p` - pointer to initialized driver instance
// `idx`    - LED number (must be 0 .. num-1)
//------------------------------------------------------------------------------
void leds_on(const leds_t * inst_p, uint32_t id);
void leds_off(const leds_t * inst_p, uint32_t id);
void leds_toggle(const leds_t * inst_p, uint32_t id);

//------------------------------------------------------------------------------
// Run meander blink (LED is ON during period_ms and OFF during period_ms)
//
// `inst_p`         - pointer to initialized driver instance
// `idx`            - LED number (must be 0 .. num-1)
// `duration_ms`    - duration of ON state in milliseconds (OFF state duration is the same)
//------------------------------------------------------------------------------
void leds_meander(const leds_t * inst_p, uint32_t idx, uint32_t duration_ms);

//------------------------------------------------------------------------------
// Run simple blink with series of pulses
//
// `inst_p`     - pointer to initialized driver instance
// `idx`        - LED number (must be 0 .. num-1)
// `series`     - number of pulses in one series
// `pulse_ms`   - duration of blink "pulse" in milliseconds
// `pause_ms`   - duration of short "pause" between pulses in milliseconds
// `period_ms`  - duration of one series in milliseconds (can be 0 for single series)
//------------------------------------------------------------------------------
void leds_blink(const leds_t * inst_p, uint32_t idx, uint8_t series, uint32_t pulse_ms, uint32_t pause_ms, uint32_t period_ms);

//------------------------------------------------------------------------------
// Run blink with extended options
//
// `inst_p`      - pointer to initialized driver instance
// `idx`         - LED number (must be 0 .. num-1)
// `series`      - number of pulses within one series
// `pulse_ms`    - duration of "pulse" in milliseconds
// `pause_ms`    - duration of "pause" between pulses in milliseconds
// `period_ms`   - duration of one series in milliseconds (can be 0 for single series)
// `delay_ms`    - delay before blink process start (can be 0)
// `is_inverted` - if 'true'  LED is ON  during the "pulse", LED is OFF during "delay", "pause", "wait"
//               - if 'false' LED is OFF during the "pulse", LED is  ON during "delay", "pause", "wait"
//------------------------------------------------------------------------------
void leds_blink_ext(const leds_t * inst_p, uint32_t idx, uint8_t series, uint32_t pulse_ms, uint32_t pause_ms,
                    uint32_t period_ms, uint32_t delay_ms, bool is_inverted);

//==================================================================================================
//============================================ TESTS ===============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Run unit-tests
// Returns - number of failed test or 0 if all tests are successful
//-----------------------------------------------------------------------------
int32_t leds_tests(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DRV_LEDS_H 
