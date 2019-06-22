#include "ATTinyDaemon.h"

/*
   ATTinyDeamon - provide a Raspberry Pi with additional data regarding a UPS
   Important: We assume that the clock frequency of the ATTiny is 8MHz. Program
   it accordingly by writing the bootloader to burn the related fuses.
*/

/*
   This variable holds the register for the I2C communication
*/
uint8_t register_number;

/*
   These are the 8 bit registers (the register numbers are defined in ATTinyDaemon.h)
   Important: The value 0xFF is no valid value and will be filtered on the RPi side
*/
uint8_t timeout             =   60;              // timeout for the reset, will be placed in eeprom (should cover shutdown and reboot)
uint8_t primed              =    0;                // 0 if turned off, 1 if primed, temporary
uint8_t should_shutdown     =    0;       // 0, all is well, 1 shutdown has been initiated, 2 and larger should shutdown
uint8_t force_shutdown      =    1;        // != 0, force shutdown if below shutdown_voltage

/*
   These are the 16 bit registers (the register numbers are defined in ATTinyDaemon.h).
   The value 0xFFFF is no valid value and will be filtered on the RPi side
*/
uint16_t bat_voltage       =    0;          // the battery voltage, 3.3 should be low and 3.7 high voltage
uint16_t bat_v_coefficient = 1000; // the multiplier for the measured battery voltage * 1000, integral non-linearity
 int16_t bat_v_constant    =    0;       // the constant added to the measurement of the battery voltage * 1000, offset error
uint16_t ext_voltage       =    0;          // the external voltage from Pi or other source
uint16_t ext_v_coefficient = 1000; // the multiplier for the measured external voltage * 1000, integral non-linearity
 int16_t ext_v_constant    =    0;       // the constant added to the measurement of the external voltage * 1000, offset error
uint16_t restart_voltage   = 3900;   // the battery voltage at which the RPi will be started again
uint16_t warn_voltage      = 3400;      // the battery voltage at which the RPi should should down
uint16_t shutdown_voltage  = 3200;  // the battery voltage at which a hard shutdown is executed
uint16_t seconds           =    0;              // seconds since last i2c access
uint16_t temperature       =    0;          // the on-chip temperature
uint16_t t_coefficient     = 1000;     // the multiplier for the measured temperature * 1000, the coefficient
 int16_t t_constant        = -270;        // the constant added to the measurement as offset

/*
   Macros for setting the pin mode and output level of the pins
*/
#define PB_OUTPUT(PIN_NAME) (DDRB |= bit(PIN_NAME))    // pinMode(PIN, OUTPUT)
#define PB_INPUT(PIN_NAME) (DDRB &= ~bit(PIN_NAME))    // pinMode(PIN, INPUT)
#define PB_HIGH(PIN_NAME) (PORTB |= bit(PIN_NAME))     // digitalWrite(PIN, 1)
#define PB_LOW(PIN_NAME) (PORTB &= ~bit(PIN_NAME))     // digitalWrite(PIN, 0)
#define PB_CHECK(PIN_NAME) (PORTB & bit(PIN_NAME))     // digitalWrite(PIN, 0)
#define PB_READ(PIN_NAME) (PINB & bit(PIN_NAME))       // digitalRead(PIN)

void setup() {
  reset_watchdog ();  // do this first in case WDT fires

  /*
     If we got a reset while pulling down the switch, this might lead to short
     spike on switch pin. Shouldn't be a problem because we can stop the RPi
     from booting in the next 2 seconds if still at shutdown level in the main
     loop. We can do this because the RPi needs more than 2 seconds to first
     access the file systems in R/W mode even in boot-optimized environments.
  */
  switch_high();

  // EEPROM, read stored data or init
  uint8_t writtenBefore;
  EEPROM.get(EEPROM_BASE_ADDRESS, writtenBefore);
  if (writtenBefore != EEPROM_INIT_VALUE) {
    // no data has been written before, initialise EEPROM
    init_EEPROM();
  } else {
    read_EEPROM_values(); //@TODO measure startup time
  }

  // Initialize I2C
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(request_event);
  Wire.onReceive(receive_event);
}

/*
   This interrupt function will be executed whenever we press the button.
   If the Raspberry is alive (i.e. no timeout) then the button
   press is interpreted as a shutdown command.
   If the Raspberry has been shutdown, primed is not set and the button
   is pressed we want to restart the Raspberry. We set primed temporarily
   to trigger a restart in the main loop.
*/
ISR (PCINT0_vect) {
  if (seconds > timeout && primed == 0) {
    primed = 1;
    // could be set during the shutdown while the timeout has not yet been exceeded. We reset it.
    should_shutdown = SL_NORMAL;
  } else {
    // signal the Raspberry that the button has been pressed.
    if (should_shutdown != SL_INITIATED) {
      should_shutdown |= SL_BUTTON;
    }
  }
}

void loop() {
  if (primed != 0 || (seconds < timeout) ) {
    // start the regular blink if either primed is set or we are not
    // yet in a timeout.
    // This means the the LED stops blinking at the same time at which
    // the second button functionality is enabled.
    ledOn_buttonOff();
  }

  read_voltages();

  if (bat_voltage > shutdown_voltage) {
    if (should_shutdown > SL_INITIATED && (seconds < timeout)) {
      // RPi should shut down. Signal by blinking 5 times
      for (int i = 0; i < 5; i++) {
        delay(BLINK_TIME);
        ledOff_buttonOn();
        delay(BLINK_TIME);
        ledOn_buttonOff();
      }
    }
  }

  // we act only if primed is set
  if (primed != 0) {
    if (bat_voltage < shutdown_voltage) {
      // immediately turn off the system if force_shutdown is set
      if (force_shutdown != 0) {
        switch_low();
      }
      // voltage will jump up if we pull the switch low, so we turn off the blinkenlight
      should_shutdown = SL_NORMAL;
      ledOff_buttonOff();
    } else if (bat_voltage > warn_voltage) {
      if (PB_CHECK(PIN_SWITCH) == 0) { // we had turned off the power to the RPi
        switch_high();
        reset_counter();
      } else {
        if (seconds > timeout) {
          // RPi has not accessed the I2C interface for more than timeout seconds.
          // We restart it. Signal restart by blinking ten times
          for (int i = 0; i < 10; i++) {
            ledOn_buttonOff();
            delay(BLINK_TIME / 2);
            ledOff_buttonOn();
            delay(BLINK_TIME / 2);
          }
          restart_raspberry();
          reset_counter();
        }
      }
    } else {
      // we are at warn_voltage and don't restart the RPi
      reset_counter();
    }
  }
  if (bat_voltage > shutdown_voltage) {
    // allow the button functionality as long as possible
    // and even if not primed
    ledOff_buttonOn();
  }

  // go to deep sleep
  // taken from http://www.gammon.com.au/power
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);
  noInterrupts ();           // timed sequence follows
  reset_watchdog();
  sleep_enable();
  interrupts ();             // guarantees next instruction executed
  sleep_cpu ();
  sleep_disable();
}

/*
   The function to reset the seconds counter. Externalized to allow for
   further functionality later on and to better communicate the intent.
*/
void reset_counter() {
  seconds = 0;
}

/*
   Read the values stored in the EEPROM. The addresses are defined in
   the header file. We use the modern get()-method that determines the
   object size itself, because the accompanying put()-method uses the
   update()-function that checks whether the data has been modified
   before it writes.
*/
void read_EEPROM_values() {
  EEPROM.get(EEPROM_TIMEOUT_ADDRESS, timeout);
  EEPROM.get(EEPROM_PRIMED_ADDRESS, primed);
  EEPROM.get(EEPROM_RESTART_V_ADDRESS, restart_voltage);
  EEPROM.get(EEPROM_WARN_V_ADDRESS, warn_voltage);
  EEPROM.get(EEPROM_SHUTDOWN_V_ADDRESS, shutdown_voltage);
  EEPROM.get(EEPROM_BAT_V_COEFFICIENT, ext_v_coefficient);
  EEPROM.get(EEPROM_BAT_V_CONSTANT, ext_v_constant);
  EEPROM.get(EEPROM_EXT_V_COEFFICIENT, ext_v_coefficient);
  EEPROM.get(EEPROM_EXT_V_CONSTANT, ext_v_constant);
  EEPROM.get(EEPROM_T_COEFFICIENT, t_coefficient);
  EEPROM.get(EEPROM_T_CONSTANT, t_constant);
}

/*
   Initialize the EEPROM and set the values to the currently
   held values in our variables. This function is called when,
   in the setup() function, we determine that no valid EEPROM
   data can be read (by checking the EEPROM_INIT_VALUE).
   This method can also be used later from the Raspberry to
   reinit the EEPROM, but individual values are written at once
   whenever they are transmitted using I2C (see the function
   receiveEvent()).
*/
void init_EEPROM() {
  // put uses update(), thus no unnecessary writes
  EEPROM.put(EEPROM_BASE_ADDRESS, EEPROM_INIT_VALUE);
  EEPROM.put(EEPROM_TIMEOUT_ADDRESS, timeout);
  EEPROM.put(EEPROM_PRIMED_ADDRESS, primed);
  EEPROM.put(EEPROM_RESTART_V_ADDRESS, restart_voltage);
  EEPROM.put(EEPROM_WARN_V_ADDRESS, warn_voltage);
  EEPROM.put(EEPROM_SHUTDOWN_V_ADDRESS, shutdown_voltage);
  EEPROM.put(EEPROM_BAT_V_COEFFICIENT, ext_v_coefficient);
  EEPROM.put(EEPROM_BAT_V_CONSTANT, ext_v_constant);
  EEPROM.put(EEPROM_EXT_V_COEFFICIENT, ext_v_coefficient);
  EEPROM.put(EEPROM_EXT_V_CONSTANT, ext_v_constant);
  EEPROM.put(EEPROM_T_COEFFICIENT, t_coefficient);
  EEPROM.put(EEPROM_T_CONSTANT, t_constant);
}
