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
  PB_HIGH(LED_BUTTON);              // Input pullup
  PB_INPUT(LED_BUTTON);

  PCMSK |= bit(LED_BUTTON);         // set interrupt pin
  GIFR |= bit(PCIF);                // clear interrupts
  GIMSK |= bit(PCIE);               // enable pin change interrupts
}

void ledOn_buttonOff() {

  GIMSK &= ~(bit(PCIE));            // disable pin change interrupts
  GIFR &= ~(bit(PCIF));             // clear interrupts
  PCMSK &= ~(bit(LED_BUTTON));      // set interrupt pin

  PB_OUTPUT(LED_BUTTON);
  PB_LOW(LED_BUTTON);
}

void ledOff_buttonOff() {
  // Go to high impedance and turn off the pin change interrupts
  GIMSK &= ~(bit(PCIE));            // disable pin change interrupts
  PB_INPUT(LED_BUTTON);
  PB_LOW(LED_BUTTON);
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
  PB_HIGH(PIN_SWITCH);
  PB_INPUT(PIN_SWITCH);
}

void switch_pin_low() {
  // Turn off pullup, then to output
  PB_LOW(PIN_SWITCH);
  PB_OUTPUT(PIN_SWITCH);
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
/*
  void restart_raspberry() {
  should_shutdown = 0;

  push_switch(reset_pulse_length);
  if (reset_configuration & 0x1) {
    // bit 0 is set, we use 2 pulses
    if (reset_configuration & 0x2) {
      // bit 1 is set, we check the external voltage
      read_voltages();

      if (ext_voltage > MIN_POWER_LEVEL) {
        // the external voltage is present i.e., the Pi has just been turned on.
        return;
      }
    }
    delay(sw_recovery_delay); // wait for the switch circuit to revover
    push_switch(reset_pulse_length);
  }
  }
*/

void restart_raspberry() {
  should_shutdown = 0;

  ups_off();
  delay(sw_recovery_delay); // wait for the switch circuit to revover
  ups_on();
}

void push_switch(uint16_t pulse_time) {
  switch_pin_low();
  delay(pulse_time);
  switch_pin_high();
}

void ups_off() {
  if (UPS_IS_VOLTAGE_CONTROLLED) {
    switch_pin_low();
  } else {
    if (UPS_CHECK_VOLTAGE) {
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
  if (UPS_IS_VOLTAGE_CONTROLLED) {
    switch_pin_high();
  } else {
    if (UPS_CHECK_VOLTAGE) {
      read_voltages();

      if (ext_voltage > MIN_POWER_LEVEL) {
        // the external voltage is present i.e., the Pi has already been turned on.
        return;
      }
    }
    push_switch(reset_pulse_length);
  }
}
