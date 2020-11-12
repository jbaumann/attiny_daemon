/*
 * The ATTiny datasheet I'm referencing is the ATTiny25/45/85 datasheet provided by Microchip.
 * Pages and Chapter numbers are for the revision Rev. 2586Q-08/13.
 */

/*
   Functions for setting the pin mode and output level of the pins.
   These will be unrolled by the compiler, so no additional overhead on the heap
*/
void pb_output(uint8_t pin) {
  DDRB |= bit(pin);              // pinMode(pin, OUTPUT)
}
void pb_input(uint8_t pin) {
  (DDRB &= ~bit(pin));           // pinMode(pin, INPUT)
}
void pb_high(uint8_t pin) {
  (PORTB |= bit(pin));           // digitalWrite(pin, 1)
}
void pb_low(uint8_t pin) {
  (PORTB &= ~bit(pin));          // digitalWrite(pin, 0)
}
/*
uint8_t PB_CHECK(uint8_t pin) {
  return (PORTB & bit(pin));     // check PIN_NAME
}
uint8_t PB_READ(uint8_t pin) {
  return (PINB & bit(pin));      // digitalRead(PIN)
}
*/



/*
   The following three methods implement the dual use of the GPIO pin
   LED_BUTTON as an LED driver and button input. This is done by
   turning on the interrupts and changing to input with pullup when
   reading the button (ledOff_buttonOn()), and turning off the interrupts
   and changing to output low when turning on the LED (ledOn_buttonOff()).
   Switching between these modes turns the LED on and off, and when the LED
   is turned off button presses are registered.
   This works even with deep sleep modes since we are using a pin change interrupt
   on this pin to check the button (see datasheet Ch.10.4.1 on p.57).
   The third method ledOff_buttonOff() turns off both LED and button sensing
   by channging LED_BUTTON to high impedance input and turning off the interrupt.
   It is used when we go into deep sleep to save as much power as possible.
*/
void ledOff_buttonOn() {
#if !defined SERIAL_DEBUG
  // switch back to monitoring button
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    pb_high(LED_BUTTON);              // First high to guarantee no spurious interrups
    pb_input(LED_BUTTON);

    PCMSK |= bit(LED_BUTTON);         // set interrupt pin
    GIFR |= bit(PCIF);                // clear interrupts
    GIMSK |= bit(PCIE);               // enable pin change interrupts
  }
#endif
}

void ledOn_buttonOff() {
#if !defined SERIAL_DEBUG
  if(led_off_mode > (volatile unsigned char) state) {
    return;
  }
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    GIMSK &= ~(bit(PCIE));            // disable pin change interrupts
    GIFR &= ~(bit(PCIF));             // clear interrupts
    PCMSK &= ~(bit(LED_BUTTON));      // disable interrupt pin

    pb_output(LED_BUTTON);
    pb_low(LED_BUTTON);
  }
#endif
}

void ledOff_buttonOff() {
#if !defined SERIAL_DEBUG
  // Go to high impedance and turn off the pin change interrupts
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    GIMSK &= ~(bit(PCIE));            // disable pin change interrupts
    pb_input(LED_BUTTON);
    pb_low(LED_BUTTON);
  }
#endif
}

/*
   The following two methods switch the PIN_SWITCH to high and low,
   respectively.
   The low state is implemented as active low output, in this mode
   the pin can drive up to 5mA (Table 21.2 p.161).
   When the ATTiny goes into deep sleep then input pins that are not
   enabled as interrupt pins, even with pull-up, are clamped to
   ground (see datasheet Ch.10.4.1 on p.57).
   Thus the high state has to be implemented as active high output.
   In this mode the pin can drive the same 5mA, which should be
   more than enough for the high-impedance EN pins used for
   switching the UPS. For instance, the Geekworm UPSv1 has two
   150K resistors pulling to Vcc resulting in 10-15uA needed to
   drive the pin (the EN pin with 5nA is irrelevant compared to
   that).
*/
void switch_pin_high() {
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    // output high
    pb_high(PIN_SWITCH);
    pb_output(PIN_SWITCH);
  }
}

void switch_pin_low() {
  // Turn off pullup, then to output
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    pb_low(PIN_SWITCH);
    pb_output(PIN_SWITCH);
  }
}

/*
   Functions that abstract checking the different bits of reset_configuration.
   These will be unrolled by the compiler, so no additional overhead on the heap
*/

boolean ups_is_voltage_controlled() {
  return ((ups_configuration & UPS_Configuration::Value::two_pulses) == 0);
}
boolean ups_is_switched() {
  return (ups_configuration & UPS_Configuration::Value::two_pulses);
}
boolean ups_no_check_voltage() {
  return ((ups_configuration & UPS_Configuration::Value::check_ext_voltage) == 0);
}
boolean ups_check_voltage() {
  return (ups_configuration & UPS_Configuration::Value::check_ext_voltage);
}

/*
   Functions that abstract the extraction of the number of additional pulses
   from the ups_configuration
 */
uint8_t ups_additional_off_pulses() {
  return (ups_configuration & 0b00110000) >> 4;
}

uint8_t ups_additional_on_pulses() {
  return (ups_configuration & 0b11000000) >> 6;
}

/*
   restartRaspberry() executes a reset of the RPI using either
   a pulse or a switching sequence, depending on reset_configuration:
   bit 0 (0 = voltage level / 1 = switched) UPS control
   bit 1 (0 = don't check / 1 = check) external voltage (only if switched)
   This leads to the following behavior:
   0    pull the switch pin low to turn the UPS off (reset needs 1 pulse)
   1    turn switch off and on to turn the UPS off/on (2 pulses, do not check for external voltage)
   2    pull the switch pin low to turn the UPS off (reset needs 1 pulse)
   3    turn switch off and on to turn the UPS off/on (2 pulses, check for external voltage)

   Additionally, should_shutdown is cleared.
*/
void restart_raspberry() {
  should_shutdown = Shutdown_Cause::none;

  ups_off();
  
  uint16_t switch_recovery_delay_safe;
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    switch_recovery_delay_safe = switch_recovery_delay;
  }
  delay(switch_recovery_delay_safe); // wait for the switch circuit to revover

  ups_on();
}

/*
 * The following function executes one push on the switch of the UPS
 */
void push_switch(uint16_t pulse_time) {
  switch_pin_low();
  
  uint16_t pulse_time_safe;
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    pulse_time_safe = pulse_time;
  }
  delay(pulse_time_safe);
  
  switch_pin_high();
}

/*
 * The following function turns the UPS off using the configuration information
 */
void ups_off() {
  if (ups_is_voltage_controlled()) {
    switch_pin_low();
  } else {
    if (ups_check_voltage()) {
      read_voltages();

      if (ext_voltage < MIN_POWER_LEVEL) {
        // the external voltage is off i.e., the Pi is already turned off.
        return;
      }
    }

    uint16_t pulse_length_safe;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
      // if pulse_length_off is set, use it. Otherwise fall back to pulse_length
      if(pulse_length_off != 0) {
        pulse_length_safe = pulse_length_off;
      } else {
        pulse_length_safe = pulse_length;
      }
    }
    push_switch(pulse_length_safe);

    uint8_t additional_pulses = ups_additional_off_pulses();

    uint16_t switch_recovery_delay_safe;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
      switch_recovery_delay_safe = switch_recovery_delay;
    }

    for(uint8_t i = 0; i < additional_pulses; i++) {
      delay(switch_recovery_delay_safe / SW_TO_PULSE_DIV); // we wait a shorter time for multiple pulses
      push_switch(pulse_length_safe);
    }

  }
}

/*
 * The following function turns the UPS on using the configuration information
 */
void ups_on() {
  if (ups_is_voltage_controlled()) {
    switch_pin_high();
  } else {
    if (ups_check_voltage()) {
      read_voltages();

      if (ext_voltage > MIN_POWER_LEVEL) {
        // the external voltage is present i.e., the Pi has already been turned on.
        return;
      }
    }
    uint16_t pulse_length_safe;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
      // if pulse_length_on is set, use it. Otherwise fall back to pulse_length
      if(pulse_length_on != 0) {
        pulse_length_safe = pulse_length_on;
      } else {
        pulse_length_safe = pulse_length;
      }
    }
    push_switch(pulse_length_safe);

    uint8_t additional_pulses = ups_additional_on_pulses();

    uint16_t switch_recovery_delay_safe;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
      switch_recovery_delay_safe = switch_recovery_delay;
    }

    for(uint8_t i = 0; i < additional_pulses; i++) {
      delay(switch_recovery_delay_safe / SW_TO_PULSE_DIV); // we wait a shorter time for multiple pulses
      push_switch(pulse_length_safe);
    }
  }
}
