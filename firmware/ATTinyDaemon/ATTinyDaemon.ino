#include "ATTinyDaemon.h"

/*
   ATTinyDeamon - provide a Raspberry Pi with additional data regarding a UPS
   Important: We assume that the clock frequency of the ATTiny is 8MHz. Program
   it accordingly by writing the bootloader to burn the related fuses.
*/

/*
   Our version number - used by the daemon to ensure that the major number is equal between firmware and daemon
 */
#if defined FLASH_8K
#define FLASH_SIZE 8192
#elif defined FLASH_4K
#define FLASH_SIZE 4096
#endif
#define MAJOR 2L
#define MINOR 1L
#define PATCH 0L

const uint32_t prog_version = (FLASH_SIZE<<24)|(MAJOR<<16)|(MINOR<<8)|PATCH;

/*
   The state variable encapsulates the all-over state of the system (ATTiny and RPi
   together).
   The possible states are:
    RUNNING_STATE      -   0 - the system is running normally
    UNCLEAR_STATE      -   1 - the system has been reset and is unsure about its state
    REC_WARN_STATE     -   2 - the system was in the warn state and is now recovering
    REC_SHUTDOWN_STATE -   4 - the system was in the shutdown state and is now recovering
    WARN_STATE         -   8 - the system is in the warn state 
    SHUTDOWN_STATE     -  16 - the system is in the shutdown state

    They are ordered in a way that allows to later check for the severity of the state by
    e.g., "if(state < SHUTDOWN_STATE)"
*/
uint8_t state = UNCLEAR_STATE;

/*
   This variable holds the register for the I2C communication
*/
uint8_t register_number;

/*
   These are the 8 bit registers (the register numbers are defined in ATTinyDaemon.h)
   Important: The value 0xFF is no valid value and will be filtered on the RPi side
*/
uint8_t timeout             =   60;  // timeout for the reset, will be placed in eeprom (should cover shutdown and reboot)
uint8_t primed              =    0;  // 0 if turned off, 1 if primed, temporary
uint8_t should_shutdown     =    0;  // 0, all is well, 1 shutdown has been initiated, 2 and larger should shutdown
uint8_t force_shutdown      =    0;  // != 0, force shutdown if below shutdown_voltage
uint8_t reset_configuration =    0;  // bit0 short/long reset pulse, bit1 1 / 2 pulse(s), bit2 check external voltage

/*
   These are the 16 bit registers (the register numbers are defined in ATTinyDaemon.h).
   The value 0xFFFF is no valid value and will be filtered on the RPi side
*/
uint16_t bat_voltage       =    0;   // the battery voltage, 3.3 should be low and 3.7 high voltage
uint16_t bat_v_coefficient = 1000;   // the multiplier for the measured battery voltage * 1000, integral non-linearity
int16_t  bat_v_constant    =    0;   // the constant added to the measurement of the battery voltage * 1000, offset error
uint16_t ext_voltage       =    0;   // the external voltage from Pi or other source
uint16_t ext_v_coefficient = 1000;   // the multiplier for the measured external voltage * 1000, integral non-linearity
int16_t  ext_v_constant    =    0;   // the constant added to the measurement of the external voltage * 1000, offset error
uint16_t restart_voltage   = 3900;   // the battery voltage at which the RPi will be started again
uint16_t warn_voltage      = 3400;   // the battery voltage at which the RPi should should down
uint16_t shutdown_voltage  = 3200;   // the battery voltage at which a hard shutdown is executed
uint16_t seconds           =    0;   // seconds since last i2c access
uint16_t temperature       =    0;   // the on-chip temperature
uint16_t t_coefficient     = 1000;   // the multiplier for the measured temperature * 1000, the coefficient
int16_t  t_constant        = -270;   // the constant added to the measurement as offset

void setup() {
  reset_watchdog ();  // do this first in case WDT fires

  /*
     If we got a reset while pulling down the switch, this might lead to short
     spike on switch pin. Shouldn't be a problem because we can stop the RPi
     from booting in the next 2 seconds if still at shutdown level in the main
     loop. We can do this because the RPi needs more than 2 seconds to first
     access the file systems in R/W mode even in boot-optimized environments.
  */

  // EEPROM, read stored data or init
  uint8_t writtenBefore;
  EEPROM.get(EEPROM_BASE_ADDRESS, writtenBefore);
  if (writtenBefore != EEPROM_INIT_VALUE) {
    // no data has been written before, initialise EEPROM
    init_EEPROM();
  } else {
    read_EEPROM_values();
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
  if(state < SHUTDOWN_STATE) {
    if (primed != 0 || (seconds < timeout) ) {
      // start the regular blink if either primed is set or we are not yet in a timeout.
      // This means the the LED stops blinking at the same time at which
      // the second button functionality is enabled.
      // We do this here to get additional on-time for the LED during reading the voltages
      ledOn_buttonOff();
    }    
  }

  read_voltages();
  handle_state();

  if (state < SHUTDOWN_STATE) {
    if (should_shutdown > SL_INITIATED && (seconds < timeout)) {
      // RPi should take action, possibly shut down. Signal by blinking 5 times
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
    if (state == SHUTDOWN_STATE) {
      // immediately turn off the system if force_shutdown is set
      if (force_shutdown != 0) {
        // we only shut down if the reset configuration is 0. 
        //@TODO: Implement for other reset configurations
        if(reset_configuration == 0) {
          switch_low();
        }
      }
      ledOff_buttonOff();
    } else if(state == WARN_STATE) {
      // The RPi has been warned using the should_shutdown variable
      // we simply let it shutdown even if it does not set SL_INITIATED
      reset_counter();
    } else if(state == REC_SHUTDOWN_STATE) {
       if (isPoweredDown()) { // we had turned off the power to the RPi
        // we only restart if the reset configuration is 0. 
        //@TODO: Implement for other reset configurations
        if(reset_configuration == 0) {
          switch_high();
        }
      }
      reset_counter();
      state = RUNNING_STATE;
    }
    
    if(state == REC_WARN_STATE) {
      state = RUNNING_STATE;
    }

    if(state == UNCLEAR_STATE) {
      // we do nothing and wait until either a timeout occurs, the voltage
      // drops to warn_voltage or is higher than restart_voltage (see handle_state())
    }
    
    if(state == RUNNING_STATE) {
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
  }
  
  if (state < SHUTDOWN_STATE) {
    // allow the button functionality as long as possible and even if not primed
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
#if defined FLASH_8K
  EEPROM.put(EEPROM_RESET_CONFIG, reset_configuration);
#endif
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
#if defined FLASH_8K
  EEPROM.put(EEPROM_RESET_CONFIG, reset_configuration);
#endif
}

boolean isPoweredDown() {
  return (PB_CHECK(PIN_SWITCH) == 0);
}
