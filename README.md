# common_drivers
**Hardware-independent drivers for timers, LEDs and buttons** 

All functions are reenterable:
- driver doesn't use internal static data
- driver's instance and table of timers/LEDs/buttons are supposed to be stored externally

## drv_swtimers
**Driver for amount of software timers based on a single tick source**

- All timers are independent and can be run in single or periodical mode
- All timers use a single hardware timer accessed over callback functions
- Each timer can call callback functions after timeout
- swtimers_task() should be called periodically from application loop to process timers' state
- swtimers_isr() should be called periodically from ISR context to provide timer ticks

## drv_leds
**Driver for amount of LEDs with configurable blinking modes**

- All LEDs are independent and can be turned on/off or configured to blink separately
- All LEDs use GPIO-outputs accessed over callback functions
- Each LED state can be switched:
  - manually in on-off mode (by calling leds_on() or leds_off() or leds_toggle() function)
  - automatically in blinking mode (swtimers_task() call internal handler by pointer)
- Depends on drv_swtimers:
  - swtimers driver should be initialized before any usage of this driver
  - swtimers_task() should be called periodically from application loop
  - swtimers_isr() should be called periodically from ISR context
  - each LED occupies one software timer

## drv_buttons
**Driver for amount of buttons with debouncing and click/hold/double click detection**

- All buttons are independent and can processed separately
- All buttons use GPIO-inputs accessed over callback functions
- Each button state can emit events:
  - pressed / released / hold / double click
  - all times are configurable
- Each button state can be updated:
  - by polling (in buttons_task() function which should be called periodically from application loop)
  - directly from ISR context (in buttons_isr() function)
- Depends on drv_swtimers:
  - swtimers driver should be initialized before any usage of this driver
  - swtimers_task() should be called periodically from application loop
  - swtimers_isr() should be called periodically from ISR context
  - each button occupies one software timer
