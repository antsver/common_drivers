// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

//**************************************************************************************************
// Buttons driver
//**************************************************************************************************
#include <string.h>
#include <assert.h>

#include "drv_buttons.h"

//==================================================================================================
//=========================================== MACROS ===============================================
//==================================================================================================

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//------------------------------------------------------------------------------
// Button state (masks can be combined with logical OR)
//------------------------------------------------------------------------------
typedef enum buttons_state_e {
    BUTTONS_STATE_IS_RELASED    = 0x00, // current button state after bouncing filter
    BUTTONS_STATE_IS_PRESSED    = 0x01,
    BUTTONS_STATE_IS_DEBOUNCING = 0x02,
    BUTTONS_STATE_IS_HOLDING    = 0x04,
    BUTTONS_STATE_IS_DOUBLE     = 0x8,
} buttons_state_t;

//------------------------------------------------------------------------------
// Button structure
//------------------------------------------------------------------------------
typedef struct button_type {

    // Settings
    buttons_handler_cb_t handler_cb;            // pointer to handler function
    uint32_t            gpio_pin;               // GPIO pin to be used in gpio_read() callback
    void*               arg_p;                  // pointer to handler argument
    uint16_t            bouncing_ms;            // bouncing filter time (if 0 - bouncing filter is disabled)
    uint16_t            double_click_ms;        // minimal double click time (if 0 - "double click" event is not generated)
    uint16_t            hold_ms;                // hold time (if 0 - "hold" event is not generated)
    buttons_check_t     check_type;             // check button state in ISR or using polling
    bool                is_pressed_low;         // pressed-low or pressed-high button
    uint8_t             timer_id;               // id of software timer to measure timeouts

    // State
    bool                is_changed;             // button state has been changed in ISR and wait to be processed (can be changed in ISR)
    bool                is_pressed_raw;         // current button state before bouncing filter (can be changed in ISR)

    //buttons_state_t     state;                  // current button state

    bool                is_pressed_debounced;   // current button state after bouncing filter
    bool                is_debouncing;          // bouncing filter timer is started
    bool                is_holding;             // hold timer is started
    bool                is_double_clicking;     // double click timer is started
} buttons_button_instance_t;

//------------------------------------------------------------------------------
// Driver instance
//------------------------------------------------------------------------------
typedef struct buttons_instance_s {
    const void*                         swtimers_p;         // pointer to software timers driver instance
    const buttons_hw_interface_t*       hw_p;               // pointer to hardware GPIO interface
    volatile buttons_button_instance_t* buttons_table_p;    // pointer to array of buttons
    uint32_t                            num;                // number of buttons
} buttons_instance_t;

//------------------------------------------------------------------------------
// Sanitizing
//------------------------------------------------------------------------------
static_assert(sizeof(buttons_button_instance_t) == sizeof(buttons_button_t), "Wrong structure size");
static_assert(sizeof(buttons_instance_t) == sizeof(buttons_t), "Wrong structure size");

//==================================================================================================
//================================ PRIVATE FUNCTIONS DECLARATIONS ==================================
//==================================================================================================

static buttons_event_t buttons_process_changes(const buttons_instance_t * buttons_inst_p, volatile buttons_button_instance_t * button_p);

//==================================================================================================
//==================================== PRIVATE STATIC DATA =========================================
//==================================================================================================

//==================================================================================================
//======================================== PUBLIC DATA =============================================
//==================================================================================================

//==================================================================================================
//================================ PUBLIC FUNCTIONS DEFINITIONS ====================================
//==================================================================================================

//------------------------------------------------------------------------------
// Init Buttons driver
//------------------------------------------------------------------------------
void buttons_init(buttons_t * inst_p, const buttons_hw_interface_t * hw_interface_p, uint32_t num,
                  volatile buttons_button_t * buttons_table_p, const swtimers_t * swtimers_p)
{
    assert((inst_p != NULL) && (swtimers_p != NULL) && (hw_interface_p != NULL) && (num > 0) && (buttons_table_p != NULL));
    buttons_instance_t * buttons_inst_p = (buttons_instance_t*)inst_p;

    memset(buttons_inst_p, 0x00, sizeof(buttons_instance_t));

    buttons_inst_p->hw_p = hw_interface_p;
    buttons_inst_p->buttons_table_p = (volatile buttons_button_instance_t*)buttons_table_p;
    buttons_inst_p->num = num;
    buttons_inst_p->swtimers_p = swtimers_p;

    memset((buttons_instance_t*)buttons_inst_p->buttons_table_p, 0x00, num * sizeof(buttons_button_instance_t));
}

//------------------------------------------------------------------------------
// Deinit Buttons driver
//------------------------------------------------------------------------------
void buttons_deinit(buttons_t * inst_p)
{
    assert(inst_p != NULL);
    buttons_instance_t * buttons_inst_p = (buttons_instance_t*)inst_p;

    // If not initialized
    if (buttons_inst_p->num == 0) {
        return;
    }

    memset((buttons_instance_t*)buttons_inst_p->buttons_table_p, 0x00, buttons_inst_p->num * sizeof(buttons_button_instance_t));
    memset(inst_p, 0x00, sizeof(buttons_instance_t));
}

//------------------------------------------------------------------------------
// Configure button
//------------------------------------------------------------------------------
void buttons_configure(const buttons_t * inst_p, uint32_t idx, uint32_t gpio_pin, uint8_t timer_id, bool is_pressed_low, buttons_check_t check_type,
                       const buttons_time_settings_t * times_p, buttons_handler_cb_t handler_cb, void * arg_p)
{
    assert(inst_p != NULL);
    const buttons_instance_t * buttons_inst_p = (const buttons_instance_t*)inst_p;
    assert(idx < buttons_inst_p->num);
    volatile buttons_button_instance_t * button_p = &(buttons_inst_p->buttons_table_p[idx]);
    const buttons_hw_interface_t * hw_p = buttons_inst_p->hw_p;

    swtimers_stop(buttons_inst_p->swtimers_p, timer_id);

    memset((buttons_button_instance_t*)button_p, 0x00, sizeof(buttons_button_instance_t));

    button_p->gpio_pin = gpio_pin;
    button_p->timer_id = timer_id;
    button_p->is_pressed_low = is_pressed_low;
    button_p->check_type = check_type;
    button_p->handler_cb = handler_cb;
    button_p->arg_p = arg_p;
    button_p->bouncing_ms = times_p->bouncing_ms;
    button_p->double_click_ms = times_p->double_click_ms;
    button_p->hold_ms = times_p->hold_ms;

    // Read current pin state
    bool gpio_state = hw_p->gpio_read(hw_p->hw_gpio_p, button_p->gpio_pin);
    button_p->is_pressed_raw = (button_p->is_pressed_low) ? (!gpio_state) : gpio_state ;
    button_p->is_pressed_debounced = button_p->is_pressed_raw;
}

//------------------------------------------------------------------------------
// Get button state after bouncing filter
//------------------------------------------------------------------------------
bool buttons_is_pressed(const buttons_t * inst_p, uint32_t idx)
{
    assert(inst_p != NULL);
    const buttons_instance_t * buttons_inst_p = (const buttons_instance_t*)inst_p;
    assert(idx < buttons_inst_p->num);

    bool result = buttons_inst_p->buttons_table_p[idx].is_pressed_debounced;
    return result;
}

//------------------------------------------------------------------------------
// Get raw button state before bouncing filter
//------------------------------------------------------------------------------
bool buttons_is_pressed_raw(const buttons_t * inst_p, uint32_t idx)
{
    assert(inst_p != NULL);
    const buttons_instance_t * buttons_inst_p = (const buttons_instance_t*)inst_p;
    assert(idx < buttons_inst_p->num);
    volatile buttons_button_instance_t * button_p = &(buttons_inst_p->buttons_table_p[idx]);
    const buttons_hw_interface_t * hw_p = buttons_inst_p->hw_p;

    // Critical section - get raw GPIO state
    if (button_p->check_type == BUTTONS_CHECK_IN_ISR) {
        hw_p->isr_disable_cb(hw_p->hw_gpio_p);
    }
    bool result = button_p->is_pressed_raw;
    if (button_p->check_type == BUTTONS_CHECK_IN_ISR) {
        hw_p->isr_enable_cb(hw_p->hw_gpio_p);
    }

    return result;
}

//------------------------------------------------------------------------------
// Poll buttons if necessary, process changes and call handlers if necessary
//------------------------------------------------------------------------------
void buttons_task(const buttons_t * inst_p)
{
    assert(inst_p != NULL);
    const buttons_instance_t * buttons_inst_p = (const buttons_instance_t*)inst_p;
    assert(buttons_inst_p->num != 0);

    for (size_t i = 0; i < buttons_inst_p->num; ++i) {

        volatile buttons_button_instance_t * button_p = &(buttons_inst_p->buttons_table_p[i]);
        buttons_event_t event = BUTTONS_NO_EVENT;

        if (button_p->check_type == BUTTONS_CHECK_DISABLED) {
            continue;
        }

        // Update button state (poll or check interruot flag)
        buttons_event_t raw_event = buttons_process_changes(buttons_inst_p, button_p);

        // If button change is detected
        if (raw_event != BUTTONS_NO_EVENT) {
            if (button_p->bouncing_ms != 0) {
                // Start debouncing - (re)start timer with debounce period
                swtimers_start_no_handler(buttons_inst_p->swtimers_p, button_p->timer_id, button_p->bouncing_ms);
                button_p->is_debouncing = true;
            }
            else {
                // Don't start debouncing, just make event (BUTTONS_PRESSED or BUTTONS_RELEASED)
                button_p->is_pressed_debounced = (raw_event & BUTTONS_PRESSED) ? true : false;
                button_p->is_holding = false;
                event |= raw_event;
            }
        }

        // Check previously started timer
        bool is_timer_run = swtimers_is_run(buttons_inst_p->swtimers_p, button_p->timer_id, NULL);

        // If debouncing is finished
        if (button_p->is_debouncing && !is_timer_run) { // - TODO - to handler
            button_p->is_debouncing = false;
            button_p->is_pressed_debounced = button_p->is_pressed_raw;
            button_p->is_holding = false;
            event |= (button_p->is_pressed_debounced) ? BUTTONS_PRESSED : BUTTONS_RELEASED;
        }

        // If hold is detected
        if (button_p->is_holding && !is_timer_run) { // - TODO - to handler
            button_p->is_holding = false;
            event |= BUTTONS_HOLD;
        }

        // If pressed - check for doublr click OR start holding
        if (event & BUTTONS_PRESSED) {
            if (button_p->is_double_clicking) {
                button_p->is_double_clicking = false;
                event |= BUTTONS_DOUBLE;
                swtimers_stop(buttons_inst_p->swtimers_p, button_p->timer_id);
            }
            else {
                swtimers_start_no_handler(buttons_inst_p->swtimers_p, button_p->timer_id, button_p->hold_ms);
                button_p->is_holding = true;
            }
        }

        // If double click timeout is over (no double click detected) - TODO - to handler
        if (button_p->is_double_clicking && !is_timer_run) {
            button_p->is_double_clicking = false;
        }

        // If released - start double click
        if (event & BUTTONS_RELEASED) {
            swtimers_start_no_handler(buttons_inst_p->swtimers_p, button_p->timer_id, button_p->double_click_ms);
            button_p->is_double_clicking = true;
        }

        // Some simultaneous events are impossible
        assert(!((event & BUTTONS_PRESSED)  && (event & BUTTONS_RELEASED)) ||
               !((event & BUTTONS_RELEASED) && (event & BUTTONS_HOLD))     ||
               !((event & BUTTONS_RELEASED) && (event & BUTTONS_DOUBLE))   ||
               !((event & BUTTONS_HOLD)     && (event & BUTTONS_DOUBLE)));

        // Finally, call handler        if ((event != BUTTONS_NO_EVENT) && (button_p->handler_cb != NULL)) {
            button_p->handler_cb(i, event, button_p->arg_p);
        }
    }
}

//------------------------------------------------------------------------------
// ISR handler for hardware GPIO pin changes
//------------------------------------------------------------------------------
void buttons_isr(const buttons_t * inst_p, uint32_t idx, bool gpio_state)
{
    const buttons_instance_t * buttons_inst_p = (const buttons_instance_t*)inst_p;
    volatile buttons_button_instance_t * button_p = &(buttons_inst_p->buttons_table_p[idx]);

    if (button_p->check_type == BUTTONS_CHECK_IN_ISR) {
        return;
    }

    gpio_state = (button_p->is_pressed_low) ? (!gpio_state) : gpio_state;
    if (button_p->is_pressed_raw != gpio_state) {
        button_p->is_pressed_raw = gpio_state;
        button_p->is_changed = true;
    }
}

//==================================================================================================
//================================ PRIVATE FUNCTIONS DEFINITIONS ===================================
//==================================================================================================

//------------------------------------------------------------------------------
// Check if button state has been changed since previous processing
// Returns - new raw state (BUTTONS_PRESSED / BUTTONS_RELEASED) or BUTTONS_NO_EVENT if no changes
//------------------------------------------------------------------------------
static buttons_event_t buttons_process_changes(const buttons_instance_t * buttons_inst_p, volatile buttons_button_instance_t * button_p)
{
    const buttons_hw_interface_t * hw_p = buttons_inst_p->hw_p;

    bool is_changed = false;
    bool is_pressed_raw;

    if (button_p->check_type == BUTTONS_CHECK_IN_POLLING) {
        // Read button state if polling is used
        bool gpio_state = hw_p->gpio_read(hw_p->hw_gpio_p, button_p->gpio_pin);
        gpio_state = (button_p->is_pressed_low) ? (!gpio_state) : gpio_state;
        if (button_p->is_pressed_raw != gpio_state) {
            button_p->is_pressed_raw = gpio_state;
            is_pressed_raw = button_p->is_pressed_raw;
            is_changed = true;
        }
    }
    else if (button_p->check_type == BUTTONS_CHECK_IN_ISR) {
        // Critical section - check if button state has been changed in ISR
        hw_p->isr_disable_cb(hw_p->hw_gpio_p);
        is_pressed_raw = button_p->is_pressed_raw;
        is_changed = button_p->is_changed;
        button_p->is_changed = false;
        hw_p->isr_enable_cb(hw_p->hw_gpio_p);
    }

    if (is_changed) {
    	return (is_pressed_raw) ? BUTTONS_PRESSED : BUTTONS_RELEASED;
    }

    return BUTTONS_NO_EVENT;
}

//------------------------------------------------------------------------------
// Handler for timeout event emitted by timer
// TODO
//------------------------------------------------------------------------------
//static void btn_timer_handler(uint32_t timer_idx, const void * app_p, void * arg_1_p, void * arg_2_p)
//{
//    assert((arg_1_p != NULL) && (arg_2_p != NULL));
//    const buttons_instance_t * buttons_inst_p = (const buttons_instance_t*)arg_1_p;
//    volatile buttons_button_instance_t * button_p = arg_2_p;
//
//    (void)timer_idx;
//    (void)app_p;
//    (void)buttons_inst_p;
//
//    // At least one timer must be started for this button
//    assert(button_p->is_debouncing || button_p->is_clicking || button_p->is_holding || button_p->is_double_clicking);
//
//    // Event from debounce timer
//    if (button_p->is_debouncing) {
//        button_p->is_debouncing = false;
//        button_p->is_pressed_debounced = button_p->is_pressed_raw;
//        button_p->event |= (button_p->is_pressed_debounced) ? BUTTONS_PRESSED : BUTTONS_RELEASED;
//    }
//}


