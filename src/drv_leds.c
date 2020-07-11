// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

//**************************************************************************************************
// LED control driver
//**************************************************************************************************
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "drv_leds.h"

//==================================================================================================
//=========================================== MACROS ===============================================
//==================================================================================================

//==================================================================================================
//========================================== TYPEDEFS ==============================================
//==================================================================================================

//------------------------------------------------------------------------------
// LED blinking states
//------------------------------------------------------------------------------
typedef enum leds_blink_e {
    LED_BLINK_STATE_DISABLED = 0,   // No blinking
    LED_BLINK_STATE_PULSE,          // Pulse
    LED_BLINK_STATE_PAUSE,          // Pause or Delay or Wait
} leds_blink_t;

//------------------------------------------------------------------------------
// Single LED structure
//------------------------------------------------------------------------------
typedef struct led_instance_s {

    // Settings
    uint32_t        gpio_pin;       // GPIO pin to be used in gpio_toggle()/gpio_write() callbacks
    uint32_t        wait_ms;        // duration form the last blink in the series to the next series in milliseconds
    uint32_t        pulse_ms;       // duration of blink pulses in milliseconds
    uint32_t        pause_ms;       // duration of short pauses between pulses in one series in milliseconds
    uint32_t        timer_id;       // id of software timer to measure blinking time
    uint8_t         series;         // number of pulses in one series
    bool            is_active_high; // active-low or active-high GPIO connection
    bool            is_inverted;    // blinking inversion - if 'true'  LED is ON  during the "pulse", LED is OFF during "delay", "pause", "wait"
                                    //                      if 'false' LED is OFF during the "pulse", LED is  ON during "delay", "pause", "wait"
    // State
    uint8_t         pulse_counter;  // counter for pulses in the current series
    leds_blink_t    blink_state;    // current state of blinking in the series
    uint8_t         align[3];

} leds_led_instance_t;

//------------------------------------------------------------------------------
// Driver instance
//------------------------------------------------------------------------------
typedef struct leds_instance_s {
    const void*                 swtimers_p;     // pointer to software timers driver instance
    const leds_hw_interface_t*  hw_p;           // pointer to hardware GPIO interface
    leds_led_instance_t*        leds_table_p;   // pointer to array of LEDs
    uint32_t                    num;            // number of LEDs
} leds_instance_t;

//------------------------------------------------------------------------------
// Sanitizing
//------------------------------------------------------------------------------
static_assert(sizeof(leds_led_instance_t) == sizeof(leds_led_t), "Wrong structure size");
static_assert(sizeof(leds_instance_t) == sizeof(leds_t), "Wrong structure size");

//==================================================================================================
//================================ PRIVATE FUNCTIONS DECLARATIONS ==================================
//==================================================================================================

static void leds_gpio_set(const leds_instance_t * inst_p, leds_led_instance_t * led_p, bool led_state);
static void leds_gpio_toggle(const leds_instance_t * inst_p, leds_led_instance_t * led_p);
static void leds_processing(uint32_t timer_idx, void * inst_p, void * led_p);

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
// Init LEDs
//------------------------------------------------------------------------------
void leds_init(leds_t * inst_p, const leds_hw_interface_t * hw_interface_p, uint32_t num, leds_led_t * leds_table_p, const swtimers_t * swtimers_p)
{
    assert((inst_p != NULL) && (swtimers_p != NULL) && (hw_interface_p != NULL) && (num > 0) && (leds_table_p != NULL));
    leds_instance_t * leds_inst_p = (leds_instance_t*)inst_p;

    memset(leds_inst_p, 0x00, sizeof(leds_instance_t));

    leds_inst_p->hw_p = hw_interface_p;
    leds_inst_p->leds_table_p = (leds_led_instance_t*)leds_table_p;
    leds_inst_p->num = num;
    leds_inst_p->swtimers_p = swtimers_p;

    memset(leds_inst_p->leds_table_p, 0x00, num * sizeof(leds_led_instance_t));

    // Stop all blinking LEDs
    for (size_t i = 0; i < leds_inst_p->num; ++i) {
        leds_inst_p->leds_table_p[i].blink_state = LED_BLINK_STATE_DISABLED;
    }
}

//------------------------------------------------------------------------------
// Deinit LED control driver
//------------------------------------------------------------------------------
void leds_deinit(leds_t * inst_p)
{
    assert(inst_p != NULL);
    leds_instance_t * leds_inst_p = (leds_instance_t*)inst_p;

    // If not initialized
    if (leds_inst_p->num == 0) {
        return;
    }

    for (size_t i = 0; i < leds_inst_p->num; ++i) {
        leds_off(inst_p, i);
    }

    memset((leds_led_instance_t*)leds_inst_p->leds_table_p, 0x00, leds_inst_p->num * sizeof(leds_led_instance_t));
    memset(leds_inst_p, 0x00, sizeof(leds_instance_t));
}

//------------------------------------------------------------------------------
// Match LED and physical GPIO pin
//------------------------------------------------------------------------------
void leds_set_pin(leds_t * inst_p, uint32_t idx, uint32_t pin_idx, uint32_t timer_idx, bool is_active_high)
{
    assert(inst_p != NULL);
    const leds_instance_t * leds_inst_p = (const  leds_instance_t*)inst_p;
    assert(idx < leds_inst_p->num);
    leds_led_instance_t * led_p = &(leds_inst_p->leds_table_p[idx]);

    led_p->gpio_pin = pin_idx;
    led_p->is_active_high = is_active_high;
    led_p->timer_id = timer_idx;
    swtimers_stop(leds_inst_p->swtimers_p, timer_idx);
}

//------------------------------------------------------------------------------
// ON / OFF / Toggle LED and stay in current blinking mode
//------------------------------------------------------------------------------
void leds_switch_on(const leds_t * inst_p, uint32_t idx)
{
    assert(inst_p != NULL);
    const leds_instance_t * leds_inst_p = (const leds_instance_t*)inst_p;
    assert(idx < leds_inst_p->num);
    leds_led_instance_t * led_p = &(leds_inst_p->leds_table_p[idx]);

    leds_gpio_set(leds_inst_p, led_p, true);
}

void leds_switch_off(const leds_t * inst_p, uint32_t idx)
{
    assert(inst_p != NULL);
    const leds_instance_t * leds_inst_p = (const leds_instance_t*)inst_p;
    assert(idx < leds_inst_p->num);
    leds_led_instance_t * led_p = &(leds_inst_p->leds_table_p[idx]);

    leds_gpio_set(leds_inst_p, led_p, false);
}

void leds_switch_toggle(const leds_t * inst_p, uint32_t idx)
{
    assert(inst_p != NULL);
    const leds_instance_t * leds_inst_p = (const leds_instance_t*)inst_p;
    assert(idx < leds_inst_p->num);
    leds_led_instance_t * led_p = &(leds_inst_p->leds_table_p[idx]);

    leds_gpio_toggle(leds_inst_p, led_p);
}

//------------------------------------------------------------------------------
// ON / OFF / Toggle LED and leave current blinking mode
//------------------------------------------------------------------------------
void leds_on(const leds_t * inst_p, uint32_t idx)
{
    assert(inst_p != NULL);
    const leds_instance_t * leds_inst_p = (const  leds_instance_t*)inst_p;
    assert(idx < leds_inst_p->num);
    leds_led_instance_t * led_p = &(leds_inst_p->leds_table_p[idx]);

    led_p->blink_state = LED_BLINK_STATE_DISABLED;
    leds_gpio_set(leds_inst_p, led_p, true);
}

void leds_off(const leds_t * inst_p, uint32_t idx)
{
    assert(inst_p != NULL);
    const leds_instance_t * leds_inst_p = (const  leds_instance_t*)inst_p;
    assert(idx < leds_inst_p->num);
    leds_led_instance_t * led_p = &(leds_inst_p->leds_table_p[idx]);

    led_p->blink_state = LED_BLINK_STATE_DISABLED;
    leds_gpio_set(leds_inst_p, led_p, false);
}

void leds_toggle(const leds_t * inst_p, uint32_t idx)
{
    assert(inst_p != NULL);
    const leds_instance_t * leds_inst_p = (const  leds_instance_t*)inst_p;
    assert(idx < leds_inst_p->num);
    leds_led_instance_t * led_p = &(leds_inst_p->leds_table_p[idx]);

    led_p->blink_state = LED_BLINK_STATE_DISABLED;
    leds_gpio_toggle(leds_inst_p, led_p);
}

//------------------------------------------------------------------------------
// Run meander blink
//------------------------------------------------------------------------------
void leds_meander(const leds_t * inst_p, uint32_t idx, uint32_t duration_ms)
{
    leds_blink_ext(inst_p, idx, 1, duration_ms, duration_ms, 2 * duration_ms, 0, false);
}

//------------------------------------------------------------------------------
// Run simple blink with series of pulses
//------------------------------------------------------------------------------
void leds_blink(const leds_t * inst_p, uint32_t idx, uint8_t series, uint32_t pulse_ms, uint32_t pause_ms, uint32_t period_ms)
{
    leds_blink_ext(inst_p, idx, series, pulse_ms, pause_ms, period_ms, 0, false);
}

//------------------------------------------------------------------------------
// Run blink with extended options
//------------------------------------------------------------------------------
void leds_blink_ext(const leds_t * inst_p, uint32_t idx, uint8_t series, uint32_t pulse_ms, uint32_t pause_ms,
                    uint32_t period_ms, uint32_t delay_ms, bool is_inverted)
{
    assert((inst_p != NULL) && (series != 0) && (pulse_ms != 0));
    const leds_instance_t * leds_inst_p = (const  leds_instance_t*)inst_p;
    assert(idx < leds_inst_p->num);
    leds_led_instance_t * led_p = &(leds_inst_p->leds_table_p[idx]);

    // Period must be either == 0 OR it must be >= than sum of all pulses and pauses within series
    assert((period_ms == 0) || (period_ms >= (pulse_ms * series) + (pause_ms * (series - 1))));

    led_p->wait_ms = (period_ms == 0) ? (0) : (period_ms - (pulse_ms * series) - (pause_ms * (series - 1)));
    led_p->pulse_ms = pulse_ms;
    led_p->pause_ms = pause_ms;
    led_p->series = series;
    led_p->pulse_counter = 0;
    led_p->is_inverted = is_inverted;

    // Set initial state or delay state
    if (delay_ms != 0) {
        // Run timer for "delay"
        swtimers_start(leds_inst_p->swtimers_p, led_p->timer_id, delay_ms, SWTIMERS_MODE_SINGLE_FROM_LOOP, &leds_processing, (void*)leds_inst_p, (void*)led_p);
        led_p->blink_state = LED_BLINK_STATE_PAUSE;
        leds_gpio_set(leds_inst_p, led_p, (is_inverted) ? true : false);
    }
    else {
        // Run timer for the first "pulse"
        swtimers_start(leds_inst_p->swtimers_p, led_p->timer_id, pulse_ms, SWTIMERS_MODE_SINGLE_FROM_LOOP, &leds_processing, (void*)leds_inst_p, (void*)led_p);
        led_p->blink_state = LED_BLINK_STATE_PULSE;
        leds_gpio_set(leds_inst_p, led_p, (is_inverted) ? false : true);
    }
}

//==================================================================================================
//================================ PRIVATE FUNCTIONS DEFINITIONS ===================================
//==================================================================================================

//------------------------------------------------------------------------------
// Internal functions for LED ON/OFF
// Calls hardware callback
//
// `inst_p` - pointer to initialized driver instance
// `led_p`  - pointer to initialized LED instance
// `state`  - new LED state, '0' - turned off, otherwise - turned on
//------------------------------------------------------------------------------
static void leds_gpio_set(const leds_instance_t * inst_p, leds_led_instance_t * led_p, bool led_state)
{
    inst_p->hw_p->gpio_write(inst_p->hw_p->hw_gpio_p, led_p->gpio_pin, (led_p->is_active_high) ? (led_state) : (!led_state));
}

//------------------------------------------------------------------------------
// Internal function for LED toggle
// Calls hardware callback
//
// `inst_p` - pointer to initialized driver instance
// `led_p`  - pointer to initialized LED instance
//------------------------------------------------------------------------------
static void leds_gpio_toggle(const leds_instance_t * inst_p, leds_led_instance_t * led_p)
{
    inst_p->hw_p->gpio_toggle(inst_p->hw_p->hw_gpio_p, led_p->gpio_pin);
}

//------------------------------------------------------------------------------
// Internal function for LED processing
// Signature corresponds to swtimers_handler_cb_t
//
// `timer_idx`  - timer index
// `inst_p`     - pointer to initialized LED driver instance (with type leds_instance_t*)
// `arg_p`      - pointer to initialized LED instance (with type leds_led_instance_t*)
//------------------------------------------------------------------------------
static void leds_processing(uint32_t timer_idx, void * inst_p, void * led_p)
{
    assert((inst_p != NULL) && (led_p != NULL));
    leds_instance_t * leds_inst_p = (leds_instance_t*)inst_p;
    leds_led_instance_t * led_inst_p = (leds_led_instance_t*)led_p;

    switch (led_inst_p->blink_state) {
        case LED_BLINK_STATE_PAUSE:
            leds_gpio_set(leds_inst_p, led_inst_p, (led_inst_p->is_inverted) ? false : true);

            // Run timer for "pulse"
            swtimers_start(leds_inst_p->swtimers_p, timer_idx, led_inst_p->pulse_ms, SWTIMERS_MODE_SINGLE_FROM_LOOP, &leds_processing, leds_inst_p, led_p);
            led_inst_p->blink_state = LED_BLINK_STATE_PULSE;
            break;

        case LED_BLINK_STATE_PULSE:
            leds_gpio_set(leds_inst_p, led_inst_p, (led_inst_p->is_inverted) ? true : false);
            led_inst_p->pulse_counter++;

            // If it was the last "pulse" in series
            if (led_inst_p->pulse_counter == led_inst_p->series) {
                led_inst_p->pulse_counter = 0;
                if (led_inst_p->wait_ms != 0) {
                    // Run timer for "wait" until the next series
                    swtimers_start(leds_inst_p->swtimers_p, timer_idx, led_inst_p->wait_ms, SWTIMERS_MODE_SINGLE_FROM_LOOP, &leds_processing, leds_inst_p, led_p);
                    led_inst_p->blink_state = LED_BLINK_STATE_PAUSE;
                }
                else {
                    // Stop blinking
                    led_inst_p->blink_state = LED_BLINK_STATE_DISABLED;
                }
            }
            else {
                // Run timer for "pause"
                swtimers_start(leds_inst_p->swtimers_p, timer_idx, led_inst_p->pause_ms, SWTIMERS_MODE_SINGLE_FROM_LOOP, &leds_processing, leds_inst_p, led_p);
                led_inst_p->blink_state = LED_BLINK_STATE_PAUSE;
            }
            break;

        case LED_BLINK_STATE_DISABLED:
            // Do nothing
            break;

        default:
            assert(0);
            break;
    }
}


