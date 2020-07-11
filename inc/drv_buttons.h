//**************************************************************************************************
// Buttons driver
//**************************************************************************************************
// Depends on drv_swtimers:
//  - swtimers driver should be initialized before any usage of this driver
//  - swtimers_task() should be called periodically from application loop
//  - swtimers_isr() should be called periodically from ISR context
//  - each button occupies one software timer
//
// Driver uses GPIO-inputs accessed over callback functions
//
// Each button state can be updated:
//  - by polling (in buttons_task() function which should be called periodically from application loop)
//  - directly from ISR context (in buttons_isr() function)
//
// All functions are reenterable:
//  - driver doesn't use internal static data
//  - driver's instance and table of buttons are supposed to be stored externally
//
//**************************************************************************************************
// Examples
//**************************************************************************************************
// Shorts for detected events at the charts below:
// e - raw rising edge before bouncing filter
// f - raw falling edge before bouncing filter
// p - "pressed" event after bouncing filter
// r - "released" event bouncing filter
// h - "hold" event
// d - "double click" event
//
// Shorts for timeouts at the charts below:
// B - bouncing filtering time (since the last edge to "press" event)
// H - hold duration (since "press" event to generated "hold" event)
// D - double click maximum duration (between "release" and "press" events)
//
//
// Example:
// bouncing filter
//    _________   _____
// __|         |_|     |____________________  unstable edges during B - no events
//   e         f e     f
//   |<- < B ->|
//
//    _   _______________   _
// __| |_|               |_| |______________  stable state during B - "press" and "release" events
//   e f e          p    f e f          r
//       |<-- =B -->|        |<-- =B -->|
//
//
//
// Example:
// single click
//    _________
// __|         |______  pulse duration shorter than H - "press" and "release" events
//   p         r
//
// Example:
// hold event
//    __________________________________
// __|                    any time      |________ stay pressed more than BTN_HOLD_MS - "hold" event
//   p               h                  r
//   |<---- =H ----->|
//
// Example:
// two independent clicks
//    _________                  _________
// __|         |________________|         |______ two clicks with gap more than D - two independent clicks
//   p         rc               p         rc
//             |<----- > D ---->|
//
// Example:
// double click event
//    _________               _______________
// __|         |_____________|    any time   |___  two clicks with gap less than D - "double click" event
//   p         rc            pd              r
//             |<--- <=D --->|
//
//
//**************************************************************************************************
#ifndef DRV_BUTTONS_H
#define DRV_BUTTONS_H

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
// Size of hidden structure buttons_button_t
//------------------------------------------------------------------------------
#define BUTTONS_SINGLE_BUTTON_INSTANCE_SIZE (28)

//------------------------------------------------------------------------------
// Size of hidden structure buttons_t
//------------------------------------------------------------------------------
#define BUTTONS_DRIVER_INSTANCE_SIZE (16)

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//------------------------------------------------------------------------------
// Button events (masks can be combined with logical OR)
//------------------------------------------------------------------------------
typedef enum buttons_event_e {
    BUTTONS_NO_EVENT =  0x00,    // no event
    BUTTONS_PRESSED  =  0x01,    // button is pressed (after bouncing filter)
    BUTTONS_RELEASED =  0x02,    // button is released (after bouncing filter)
    BUTTONS_HOLD     =  0x04,    // hold is detected
    BUTTONS_DOUBLE   =  0x8,     // two short clicks are detected
} buttons_event_t;

//------------------------------------------------------------------------------
// Button check mode - in ISR or using polling from application
//------------------------------------------------------------------------------
typedef enum buttons_check_e {
    BUTTONS_CHECK_DISABLED = 0,  // disabled button - no polling, no events
    BUTTONS_CHECK_IN_POLLING,    // driver polls button in buttons_task function
    BUTTONS_CHECK_IN_ISR,        // application must call buttons_isr function when GPIO pin is changed
} buttons_check_t;

//------------------------------------------------------------------------------
// Callback - Button event handler (to be called to notify application about event)
//
// `button_idx` - index of SW timer
// `event`      - mask with button events, combined with logical OR
// `arg_p`      - pointer to application data, passed over buttons_set_handler function (can be NULL)
//------------------------------------------------------------------------------
typedef void (*buttons_handler_cb_t)(uint32_t button_idx, buttons_event_t event, void * arg_p);

//------------------------------------------------------------------------------
// Callback - Read hardware GPIO state
//
// `hw_gpio_p` - pointer to hardware GPIO driver, passed over buttons_hw_interface_t structure
// `pin_idx`   - index of hardware GPIO pin, passed over buttons_set_pin function
//
// Returns - 'false' - input is logical zero, 'true' - input is logical one
//------------------------------------------------------------------------------
typedef bool (*buttons_gpio_read_cb_t)(void * hw_gpio_p, uint32_t pin_idx);

//------------------------------------------------------------------------------
// Callback - Enable interrupt from hardware GPIO pin (to allow buttons_isr handler calls)
// Callback - Disable interrupt from hardware GPIO pin (to allow buttons_isr handler calls)
//
// `hw_timer_p` - pointer to hardware GPIO driver, passed over buttons_hw_interface_t structure (can be NULL)
//------------------------------------------------------------------------------
typedef void (*buttons_isr_ctrl_cb_t)(void * hw_gpio_p);

//------------------------------------------------------------------------------
// Interface to hardware GPIO
//------------------------------------------------------------------------------
typedef struct buttons_hw_interface_s {
    void*                       hw_gpio_p;      // Pointer to hardware GPIO driver to be passed into callbacks (can be NULL)
    buttons_isr_ctrl_cb_t       isr_enable_cb;  // Enable interrupt from hardware timer
    buttons_isr_ctrl_cb_t       isr_disable_cb; // Disable interrupt from hardware timer
    buttons_gpio_read_cb_t      gpio_read;      // Read hardware GPIO state
} buttons_hw_interface_t;

//------------------------------------------------------------------------------
// Time settings
// `bouncing_ms`    - bouncing filter time in milliseconds (if 0 - bouncing filter is disabled)
// `double_click_ms`- minimal timeout in milliseconds between clicks to generate "double click" event (if 0 - "double click" event is not generated)
// `hold_ms`        - timeout in milliseconds to generate "hold" event (if 0 - "hold" event is not generated)
//------------------------------------------------------------------------------
typedef struct buttons_time_settings_s {
    uint16_t bouncing_ms;
    uint16_t double_click_ms;
    uint16_t hold_ms;
} buttons_time_settings_t;

//------------------------------------------------------------------------------
// Single button instance (structure is hidden in .c file)
//------------------------------------------------------------------------------
typedef struct buttons_button_s {
    uint8_t data[BUTTONS_SINGLE_BUTTON_INSTANCE_SIZE];
} buttons_button_t;

//------------------------------------------------------------------------------
// Driver instance (structure is hidden in .c file)
//------------------------------------------------------------------------------
typedef struct buttons_s {
    uint8_t data[BUTTONS_DRIVER_INSTANCE_SIZE];
} buttons_t;

//==================================================================================================
//================================ PUBLIC FUNCTIONS DECLARATIONS ===================================
//==================================================================================================

//------------------------------------------------------------------------------
// Init Buttons driver

// `inst_p`         - pointer to driver instance, can be uninitialized
// `hw_interface_p` - pointer to driver's hardware interface (structure must be alive until deinitialization of the driver)
// `num`            - number of buttons (must be > 0)
// `buttons_table_p`- pointer to volatile array of timers with size = num * sizeof(buttons_button_t) bytes
// `swtimers_p`     - pointer to initialized software timers driver instance
// `app_p`          - pointer to application to be passed to handler callback (can be NULL)
//------------------------------------------------------------------------------
void buttons_init(buttons_t * inst_p, const buttons_hw_interface_t * hw_interface_p, uint32_t num,
                  volatile buttons_button_t * buttons_table_p, const swtimers_t * swtimers_p);

//------------------------------------------------------------------------------
// Deinit Buttons driver
//
// `inst_p` - pointer to initialized driver instance
//------------------------------------------------------------------------------
void buttons_deinit(buttons_t * inst_p);

//------------------------------------------------------------------------------
// Configure button
// Matches button and physical GPIO pin
// Sets timing settings and handler callback
//
// `inst_p`         - pointer to initialized driver instance
// `idx`            - button number (must be 0 .. num-1)
// `timer_id`       - software timer to measure bouncing and click times
// `is_pressed_low` - 'true' if button is pressed-low, 'false' if button is pressed-high
// `check_type`     - button check method - polling from application buttons_task() or from ISR buttons_isr()
// `times_p`        - pointer to timing settings (all settings are copied into button instance)
// `handler_cb`     - pointer to handler callback (can be NULL)
// `arg_p`          - pointer to application data to be passed into handler callback (can be NULL)
//------------------------------------------------------------------------------
void buttons_configure(const buttons_t * inst_p, uint32_t idx, uint32_t gpio_pin, uint8_t timer_id, bool is_pressed_low, buttons_check_t check_type,
                       const buttons_time_settings_t * times_p, buttons_handler_cb_t handler_cb, void * arg_p);

//------------------------------------------------------------------------------
// Get button state after bouncing filter
// If bouncing filter is disabled - returns raw pin state
//
// `inst_p`         - pointer to initialized driver instance
// `idx`            - button number (must be 0 .. num-1)
//
// Returns - 'true' if button is pressed, 'false' otherwise
//------------------------------------------------------------------------------
bool buttons_is_pressed(const buttons_t * inst_p, uint32_t idx);

//------------------------------------------------------------------------------
// Get raw button state before bouncing filter
//
// `inst_p`         - pointer to initialized driver instance
// `idx`            - button number (must be 0 .. num-1)
//
// Returns - 'true' if button is pressed, 'false' otherwise
//------------------------------------------------------------------------------
bool buttons_is_pressed_raw(const buttons_t * inst_p, uint32_t idx);

//------------------------------------------------------------------------------
// Poll buttons if necessary, process changes and call handlers if necessary
//
// To be called periodically from main loop
//
// `inst_p` - pointer to initialized driver instance
//------------------------------------------------------------------------------
void buttons_task(const buttons_t * inst_p);

//------------------------------------------------------------------------------
// ISR handler for hardware GPIO pin changes
// Button must be configured as BUTTONS_CHECK_IN_ISR
//
// To be called from ISR if GPIO pin change is detected
//
// `inst_p`         - pointer to initialized driver instance
// `idx`            - button number (must be 0 .. num-1)
// `gpio_state`     - 'false' - input is logical zero, 'true' - input is logical one
//------------------------------------------------------------------------------
void buttons_isr(const buttons_t * inst_p, uint32_t idx, bool gpio_state);

//==================================================================================================
//============================================ TESTS ===============================================
//==================================================================================================

//-----------------------------------------------------------------------------
// Run unit-tests
// Returns - number of failed test or 0 if all tests are successful
//-----------------------------------------------------------------------------
int32_t buttons_tests(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DRV_BUTTONS_H 
