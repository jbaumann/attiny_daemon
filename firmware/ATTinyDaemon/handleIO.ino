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
   The third method ledOff_buttonOff() turns off both LED and button sensing
   by channging LED_BUTTON to high impedance input and turning off the interrupt.
   It is used when we go into deep sleep to save as much power as possible.
*/
void ledOff_buttonOn() {
  // switch back to monitoring button
  pb_input(LED_BUTTON);
  pb_high(LED_BUTTON);              // Input pullup

  PCMSK |= bit(LED_BUTTON);         // set interrupt pin
  GIFR |= bit(PCIF);                // clear interrupts
  GIMSK |= bit(PCIE);               // enable pin change interrupts
}

void ledOn_buttonOff() {
  if(led_off_mode) {
    return;
  }
  GIMSK &= ~(bit(PCIE));            // disable pin change interrupts
  GIFR &= ~(bit(PCIF));             // clear interrupts
  PCMSK &= ~(bit(LED_BUTTON));      // set interrupt pin

  pb_output(LED_BUTTON);
  pb_low(LED_BUTTON);
}

void ledOff_buttonOff() {
  // Go to high impedance and turn off the pin change interrupts
  GIMSK &= ~(bit(PCIE));            // disable pin change interrupts
  pb_input(LED_BUTTON);
  pb_low(LED_BUTTON);
}

/*
   The following two methods switch the PIN_SWITCH to high and low,
   respectively. The high state is implemented using the pullup resistor
   of the ATTiny. This might be changed to an active high output later.
   For the Geekworm UPS this is irrelevant because the EN pin we are driving
   takes around 5nA and the two 150K resistors pulling the pin to GND take
   another 10-15uA. The low state is implemented by simply pulling the output
   low.
*/
void switch_pin_high() {
  // Input with Pullup
  pb_high(PIN_SWITCH);
  pb_input(PIN_SWITCH);
}

void switch_pin_low() {
  // Turn off pullup, then to output
  pb_low(PIN_SWITCH);
  pb_output(PIN_SWITCH);
}

/*
   Functions that abstract checking the different bits of reset_configuration.
   These will be unrolled by the compiler, so no additional overhead on the heap
*/

boolean ups_is_voltage_controlled() {
  return ((reset_configuration & 0x1) == 0);
}
boolean ups_is_switched() {
  return (reset_configuration & 0x1);
}
boolean ups_no_check_voltage() {
  return ((reset_configuration & 0x2) == 0);
}
boolean ups_check_voltage() {
  return (reset_configuration & 0x2);
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
  should_shutdown = 0;

  ups_off();
  delay(switch_recovery_delay); // wait for the switch circuit to revover
  ups_on();
}

void push_switch(uint16_t pulse_time) {
  switch_pin_low();
  delay(pulse_time);
  switch_pin_high();
}

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
    push_switch(reset_pulse_length);
  }
}

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
    push_switch(reset_pulse_length);
  }
}
