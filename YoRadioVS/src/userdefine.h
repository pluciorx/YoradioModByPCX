#pragma once

// call these from your project's setup/loop hooks
void mpr121_setup();
void mpr121_loop();

// optocoupler helpers
void optocouplers_setup();
void opto_input_selector_pulse();          // pin 1, fixed 20ms pulse
uint8_t opto_input_selector_cycle();       // returns 0=RADIO, 1=BT, 2=AUX
uint8_t opto_input_selector_current();     // current tracked state: 0=RADIO, 1=BT, 2=AUX
uint8_t opto_input_selector_set(uint8_t targetState);
uint8_t opto_input_selector_step(bool toRight);
bool    opto_input_selector_flush(uint8_t *appliedState = nullptr); // call from display loop to apply latest runtime input state
void opto_aux1_pulse(uint16_t durationMs); // pin 2
void opto_aux2_pulse(uint16_t durationMs); // pin 40
void opto_aux3_pulse(uint16_t durationMs); // pin 39
void toggleOpto(uint8_t pin, bool state);            // helper for direct on/off control (non-pulsed)
// optional app-level helpers you will implement later
void yoradio_save_favorite(uint8_t btn);
void yoradio_goto_favorite(uint8_t btn);