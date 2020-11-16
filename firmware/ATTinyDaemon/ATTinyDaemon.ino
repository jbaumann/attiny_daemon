#include "ATTinyDaemon.h"

/*
   ATTinyDeamon - provide a Raspberry Pi with additional data regarding a UPS
   Important: We assume that the clock frequency of the ATTiny is 8MHz. Program
   it accordingly by writing the bootloader to burn the related fuses.
*/

/*
   Store major and minor version and the patch level in a constant
 */
const uint32_t prog_version = (MAJOR << 16) | (MINOR << 8) | PATCH;

/*
   The state variable encapsulates the all-over state of the system (ATTiny and RPi
   together).
   The possible states are:
    RUNNING_STATE       -  0 - the system is running normally
    UNCLEAR_STATE       -  1 - the system has been reset and is unsure about its state
    WARN_TO_RUNNING     -  2 - the system transitions from warn state to running state
    SHUTDOWN_TO_RUNNING -  4 - the system transitions from shutdown state to running state
    WARN_STATE          -  8 - the system is in the warn state
    WARN_TO_SHUTDOWN    - 16 - the system transitions from warn state to shutdown state
    SHUTDOWN_STATE      - 32 - the system is in the shutdown state

    They are ordered in a way that allows to later check for the severity of the state by
    e.g., "if(state <= WARN_STATE)"
*/
volatile State state = State::unclear_state;

/*
   This variable holds the register for the I2C communication. Declaration in handleI2C.
*/
extern Register register_number;

/*
   These variables hold the fuse settings. If we try to read the fuse settings over I2C without
   this buffering then we very often get timeouts. So, until we are in dire need of memory we
   simply copy the fuse settings to RAM.
 */
uint8_t fuse_low;
uint8_t fuse_high;
uint8_t fuse_extended;

/*
   These are the 8 bit registers (the register numbers are defined in ATTinyDaemon.h)
   Important: The value 0xFFFF is no valid value and will be filtered on the RPi side
*/
volatile uint8_t timeout                  =   60;  // timeout for the reset, will be placed in eeprom (should cover shutdown and reboot)
volatile uint8_t primed                   =    0;  // 0 if turned off, 1 if primed, temporary
volatile uint8_t should_shutdown          = Shutdown_Cause::none; 
volatile uint8_t force_shutdown           =    0;  // != 0, force shutdown if below shutdown_voltage
volatile uint8_t ups_configuration        =    0;  // bit 0 (0 = 1 / 1 = 2) pulses, bit 1 (0 = don't check / 1 = check) external voltage (only if 2 pulses)
volatile uint8_t led_off_mode             =    0;  // 0 LED behaves normally, higher values blink LED only in warn state
volatile uint8_t vext_off_is_shutdown     =    0;  // 0 normal timeout is used, 1 shutdown only if Vext is 0V
/*
   These are the 16 bit registers (the register numbers are defined in ATTinyDaemon.h).
   The value 0xFFFFFFFF is no valid value and will be filtered on the RPi side
*/
volatile uint16_t bat_voltage             =    0;   // the battery voltage, 3.3 should be low and 3.7 high voltage
volatile uint16_t bat_voltage_coefficient = 1000;   // the multiplier for the measured battery voltage * 1000, integral non-linearity
volatile int16_t  bat_voltage_constant    =    0;   // the constant added to the measurement of the battery voltage * 1000, offset error
volatile uint16_t ext_voltage             =    0;   // the external voltage from Pi or other source
volatile uint16_t ext_voltage_coefficient = 2000;   // the multiplier for the measured external voltage * 1000, integral non-linearity
volatile int16_t  ext_voltage_constant    =  700;   // the constant added to the measurement of the external voltage * 1000, offset error
volatile uint16_t restart_voltage         = 3900;   // the battery voltage at which the RPi will be started again
volatile uint16_t warn_voltage            = 3400;   // the battery voltage at which the RPi should should down
volatile uint16_t ups_shutdown_voltage    = 3200;   // the battery voltage at which a hard shutdown is executed
volatile uint16_t seconds                 =    0;   // seconds since last i2c access
volatile uint16_t temperature             =    0;   // the on-chip temperature
volatile uint16_t temperature_coefficient = 1000;   // the multiplier for the measured temperature * 1000, the coefficient
volatile int16_t  temperature_constant    = -270;   // the constant added to the measurement as offset
volatile uint16_t pulse_length            =  200;   // the ups pulse length (normally 200 for a reset, 4000 for switching)
volatile uint16_t pulse_length_on         =    0;   // if set is used as the pulse_length for turning the UPS on (for switched UPS)
volatile uint16_t pulse_length_off        =    0;   // if set is used as the pulse_length for turning the UPS off (for switched UPS)
volatile uint16_t switch_recovery_delay   = 1000;   // the pause needed between two reset pulse for the circuit recovery

/*
   This variable holds a copy of the mcusr register allowing is to inspect the cause for the
   last reset.
 */
uint8_t mcusr_mirror = 0;

/*
   This variable signals that I2C registers that are stored in the EEPROM have been updated.
   This happens in the I2C receive_event() function. Setting this variable leads to a call
   to the write_EEPROM() function in the main loop.
 */
volatile bool update_eeprom = false;

/*
   This variable signals that the bat voltage has to be reset since coefficient or constant
   have been changed
 */
volatile uint8_t reset_bat_voltage = false;
 
void setup() {
  mcusr_mirror = MCUSR;
  reset_watchdog ();  // do this first in case WDT fires

  // pull the switch pin to high to ensure that we do not turn off the UPS by accident
  switch_pin_high();

#if defined SERIAL_DEBUG
  initTXPin();
  useCliSeiForStrings(false);
  Serial.println(F("In setup()"));
#endif

  check_fuses();      // verify that we can run with the fuse settings

  /*
     If we got a reset while pulling down the switch, this might lead to short
     spike on switch pin. Shouldn't be a problem because we can stop the RPi
     from booting in the next 2 seconds if still at shutdown level in the main
     loop. We can do this because the RPi needs more than 2 seconds to first
     access the file systems in R/W mode even in boot-optimized environments.
  */

  // EEPROM, read stored data or init
  read_or_init_EEPROM();

  // Initialize I2C
  init_I2C();
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

  disable_watchdog();
  
  if (seconds > timeout && primed != 1) {
    primed = 1;
    // could be set during the shutdown while the timeout has not yet been exceeded. We reset it.
    should_shutdown = Shutdown_Cause::none;
  } else {
    // signal the Raspberry that the button has been pressed.
    should_shutdown |= Shutdown_Cause::button;
  }
}

void loop() {
  handle_state();
  if(update_eeprom) {
    update_eeprom = false;
    write_EEPROM();
  }

  handle_sleep();
}

/*
   handle_sleep() sends the ATTiny to deep sleep for 1, 2 or 8 seconds 
   (determined in handle_watchdog()) depending on the state (normal, warn, shutdown).
   Since we are going into SLEEP_MODE_PWR_DOWN all power domains are shut down and
   only a very few wake-up sources are still active (USI Start Condition, Watchdog
   Interrupt, INT0 and Pin Change). See data sheet ch. 7.1, p. 34.
   Taken in part from http://www.gammon.com.au/power
 */
void handle_sleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  noInterrupts();           // timed sequence follows
  reset_watchdog();
  sleep_enable();
  sleep_bod_disable();
  interrupts();             // guarantees next instruction executed
  sleep_cpu();
  sleep_disable();  
}

/*
   The function to reset the seconds counter. Externalized to allow for
   further functionality later on and to better communicate the intent.
   Suffix Int means the function should be used in an interrupt, suffix
   Safe means that it is safe to use anywhere.
*/
void inline reset_counter_Int() {
  seconds = 0;
}

void inline reset_counter_Safe() {
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    reset_counter_Int();
  }
}

/*
   check the fuse settings for clock frequency and divisor. Signal SOS in
   an endless loop if not correct.
*/
void check_fuses() {  
#if defined SERIAL_DEBUG
  Serial.println(F("In check_fuses()"));
#endif

  fuse_low = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS);
  fuse_high = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
  fuse_extended = boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS);

  // check low fuse, ignore the fuse settings for the startup time
  if ( (fuse_low & FUSE_SUT0 & FUSE_SUT1)== 0xC2) {

    // everything set up perfectly, we are running at 8MHz
    return;
  }
  if (fuse_low == 0x62) {
    /*
       Default fuse setting, we can change the clock divisor to run with 8 MHz.
       We do not need to correct any other values for the arduino libraries
       since we only use the delay()-function which relies on a system timer 
     */
    
    clock_prescale_set(clock_div_1);
    return;
  }
  // fuses have been changed, but not to the needed frequency. We send an SOS.
  
#if defined SERIAL_DEBUG
  Serial.println(F("Wrong fuse settings"));
#endif

  while (1) {
    blink_led(3, BLINK_TIME / 2);
    delay(BLINK_TIME / 2);
    blink_led(3, BLINK_TIME);
    delay(BLINK_TIME / 2);
    blink_led(3, BLINK_TIME / 2);
    delay(BLINK_TIME);
  }
}

/*
   Blink the led n times for blink_length 
 */
void blink_led(uint8_t n, uint8_t blink_length) {
    for (uint8_t i = 0; i < n; i++) {
      ledOn_buttonOff();
      delay(blink_length);
      ledOff_buttonOn();
      delay(blink_length);
    }  
}
