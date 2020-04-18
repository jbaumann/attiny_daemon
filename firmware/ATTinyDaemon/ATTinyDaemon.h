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
 * Flash size definition
 * used to decide which implementation fits into the flash
 * We define FLASH_4K (ATTiny45) and FLASH_8K (ATTiny85 and larger)
 */
#if defined (__AVR_ATtiny25__)
#error "This processor does not have enough flash for this program"
#elif defined (__AVR_ATtiny45__)
#error "This processor does not have enough flash for this program"
#elif defined (__AVR_ATtiny85__)
#define FLASH_8K
#endif


/*
 * Pinout
 * 1  !RESET ADC0 PCINT5  PB5
 * 2         ADC3 PCINT3  PB3
 * 3         ADC2 PCINT4  PB4
 * 4  GND
 * 5  SDA    AREF  AIN0  MOSI
 * 6         AIN1 PCINT1  PB1
 * 7  SCK    ADC1 PCINT2  PB2   INT0
 * 8  VCC
 * 
 */


/*
   Definition for the different pins
 */
const uint8_t LED_BUTTON        =   PB4;    // combined led/button pin
const uint8_t PIN_SWITCH        =   PB1;    // pin used for pushing the switch (the normal way to reset the RPi)
const uint8_t PIN_RESET         =   PB5;    // Reset pin (used as an alternative direct way to reset the RPi)
/*
   The following is needed as a define statement to allow the macro expansion in handleVoltages.ino
 */
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
enum State {
  RUNNING_STATE                 = 0,       // the system is running normally
  UNCLEAR_STATE                 = bit(0),  // the system has been reset and is unsure about its state
  WARN_TO_RUNNING               = bit(1),  // the system transitions from warn state to running state
  SHUTDOWN_TO_RUNNING           = bit(2),  // the system transitions from shutdown state to running state
  WARN_STATE                    = bit(3),  // the system is in the warn state
  WARN_TO_SHUTDOWN              = bit(4),  // the system transitions from warn state to shutdown state
  SHUTDOWN_STATE                = bit(5),  // the system is in the shutdown state
} __attribute__ ((__packed__));            // force smallest size i.e., uint_8t (GCC syntax)

/*
   EEPROM address definition
   Base address is used to decide whether data was stored before using a special init value
   Following this is the data   
 */
enum EEPROM_Address {
  EEPROM_BASE_ADDRESS           =  0,      // uint8_t
  EEPROM_TIMEOUT_ADDRESS        =  1,      // uint8_t
  EEPROM_PRIMED_ADDRESS         =  2,      // uint8_t
  EEPROM_FORCE_SHUTDOWN         =  3,      // uint8_t
  EEPROM_RESTART_V_ADDRESS      =  4,      // uint16_t
  EEPROM_WARN_V_ADDRESS         =  6,      // uint16_t
  EEPROM_SHUTDOWN_V_ADDRESS     =  8,      // uint16_t
  EEPROM_BAT_V_COEFFICIENT      = 10,      // uint16_t
  EEPROM_BAT_V_CONSTANT         = 12,      // uint16_t
  EEPROM_EXT_V_COEFFICIENT      = 14,      // uint16_t
  EEPROM_EXT_V_CONSTANT         = 16,      // uint16_t
  EEPROM_T_COEFFICIENT          = 18,      // uint16_t
  EEPROM_T_CONSTANT             = 20,      // uint16_t
  EEPROM_RESET_CONFIG           = 22,      // uint8_t
  EEPROM_RESET_PULSE_LENGTH     = 23,      // uint16_t
  EEPROM_SW_RECOVERY_DELAY      = 25,      // uint16_t
  EEPROM_LED_OFF_MODE           = 27,      // uint8_t
} __attribute__ ((__packed__));            // force smallest size i.e., uint_8t (GCC syntax)

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
 /*
#define SL_NORMAL                0
#define SL_RESERVED_0            bit(0)
#define SL_INITIATED             bit(1)
#define SL_EXT_V                 bit(2)
#define SL_BUTTON                bit(3)
#define SL_RESERVED_4            bit(4)
// the following levels definitely trigger a shutdown
#define SL_RESERVED_5            bit(5)
#define SL_RESERVED_6            bit(6)
#define SL_BAT_V                 bit(7)
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
