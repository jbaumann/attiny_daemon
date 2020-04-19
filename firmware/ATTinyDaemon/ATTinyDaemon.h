#include <USIWire.h>
#include <util/atomic.h>
#include <EEPROM.h>
#include <limits.h>
#include <avr/io.h>
#include <avr/boot.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

/*
   Flash size definition
   used to decide which implementation fits into the flash
   We define FLASH_4K (ATTiny45) and FLASH_8K (ATTiny85 and larger)
*/
#if defined (__AVR_ATtiny25__)
#error "This processor does not have enough flash for this program"
#elif defined (__AVR_ATtiny45__)
#error "This processor does not have enough flash for this program"
#elif defined (__AVR_ATtiny85__)
#define FLASH_8K
#endif


/*
   Definition for the different pins

   Pinout
   1  !RESET ADC0 PCINT5  PB5
   2         ADC3 PCINT3  PB3
   3         ADC2 PCINT4  PB4
   4  GND
   5  SDA    AREF  AIN0  MOSI
   6         AIN1 PCINT1  PB1
   7  SCK    ADC1 PCINT2  PB2   INT0
   8  VCC
*/
const uint8_t LED_BUTTON        =   PB4;    // combined led/button pin
const uint8_t PIN_SWITCH        =   PB1;    // pin used for pushing the switch (the normal way to reset the RPi)
const uint8_t PIN_RESET         =   PB5;    // Reset pin (used as an alternative direct way to reset the RPi)
// The following pin definition is needed as a define statement to allow the macro expansion in handleVoltages.ino
#define EXT_VOLTAGE                ADC3    // ADC number, used to measure external or RPi voltage (Ax, ADCx or x)

/*
   Basic constants
*/
const uint8_t  BLINK_TIME       =    100;  // time in milliseconds for the LED to blink
const uint16_t MIN_POWER_LEVEL  =   4750;  // the voltage level seen as "ON" at the external voltage after a reset
const uint8_t  NUM_MEASUREMENTS =      5;  // the number of ADC measurements we average, should be larger than 4


/*
   Values modelling the different states the system can be in
*/
enum class State : uint8_t {
  running_state                 = 0,       // the system is running normally
  unclear_state                 = bit(0),  // the system has been reset and is unsure about its state
  warn_to_running               = bit(1),  // the system transitions from warn state to running state
  shutdown_to_running           = bit(2),  // the system transitions from shutdown state to running state
  warn_state                    = bit(3),  // the system is in the warn state
  warn_to_shutdown              = bit(4),  // the system transitions from warn state to shutdown state
  shutdown_state                = bit(5),  // the system is in the shutdown state
};

/*
   EEPROM address definition
   Base address is used to decide whether data was stored before using a special init value
   Following this is the data
*/
namespace EEPROM_Address {
  // this enum is in its own namespace and not declared as a class to keep the implicit conversion
  // to int when calling EEPROM.get() or EEPROM.put(). The result, when using it, is the same.
  enum EEPROM_Address {
    base                        =  0,      // uint8_t
    timeout                     =  1,      // uint8_t
    primed                      =  2,      // uint8_t
    force_shutdown              =  3,      // uint8_t
    restart_voltage             =  4,      // uint16_t
    warn_voltage                =  6,      // uint16_t
    shutdown_voltage            =  8,      // uint16_t
    bat_voltage_coefficient     = 10,      // uint16_t
    bat_voltage_constant        = 12,      // uint16_t
    ext_voltage_coefficient     = 14,      // uint16_t
    ext_voltage_constant        = 16,      // uint16_t
    temperature_coefficient     = 18,      // uint16_t
    temperature_constant        = 20,      // uint16_t
    reset_configuration         = 22,      // uint8_t
    reset_pulse_length          = 23,      // uint16_t
    switch_recovery_delay       = 25,      // uint16_t
    led_off_mode                = 27,      // uint8_t
  }; __attribute__ ((__packed__));            // force smallest size i.e., uint_8t (GCC syntax)
}

const uint8_t EEPROM_INIT_VALUE = 0x42;

/*
   I2C interface and register definitions
*/
const uint8_t I2C_ADDRESS       = 0x37;

enum Register {
  REGISTER_LAST_ACCESS          = 0x01,
  REGISTER_BAT_VOLTAGE          = 0x11,
  REGISTER_EXT_VOLTAGE          = 0x12,
  REGISTER_BAT_V_COEFFICIENT    = 0x13,
  REGISTER_BAT_V_CONSTANT       = 0x14,
  REGISTER_EXT_V_COEFFICIENT    = 0x15,
  REGISTER_EXT_V_CONSTANT       = 0x16,
  REGISTER_TIMEOUT              = 0x21,
  REGISTER_PRIMED               = 0x22,
  REGISTER_SHOULD_SHUTDOWN      = 0x23,
  REGISTER_FORCE_SHUTDOWN       = 0x24,
  REGISTER_LED_OFF_MODE         = 0x25,
  REGISTER_RESTART_VOLTAGE      = 0x31,
  REGISTER_WARN_VOLTAGE         = 0x32,
  REGISTER_SHUTDOWN_VOLTAGE     = 0x33,
  REGISTER_TEMPERATURE          = 0x41,
  REGISTER_T_COEFFICIENT        = 0x42,
  REGISTER_T_CONSTANT           = 0x43,
  REGISTER_RESET_CONFIG         = 0x51,
  REGISTER_RESET_PULSE_LENGTH   = 0x52,
  REGISTER_SW_RECOVERY_DELAY    = 0x53,
  REGISTER_VERSION              = 0x80,
  REGISTER_FUSE_LOW             = 0x81,
  REGISTER_FUSE_HIGH            = 0x82,
  REGISTER_FUSE_EXTENDED        = 0x83,
  REGISTER_INTERNAL_STATE       = 0x84,

  REGISTER_INIT_EEPROM          = 0xFF,
} __attribute__ ((__packed__));            // force smallest size i.e., uint_8t (GCC syntax)


/*
   The shutdown levels
*/
enum Shutdown_Levels {
  SL_NORMAL                     = 0,
  SL_RESERVED_0                 = bit(0),
  SL_INITIATED                  = bit(1),
  SL_EXT_V                      = bit(2),
  SL_BUTTON                     = bit(3),
  SL_RESERVED_4                 = bit(4),
  // the following levels definitely trigger a shutdown
  SL_RESERVED_5                 = bit(5),
  SL_RESERVED_6                 = bit(6),
  SL_BAT_V                      = bit(7),

} __attribute__ ((__packed__));            // force smallest size i.e., uint_8t (GCC syntax)
