#include <USIWire.h>
#include <util/atomic.h>
#include <EEPROM.h>
#include <limits.h>
#include <avr/io.h>
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
#define FLASH_4K
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
   Macros for setting the pin mode and output level of the pins
*/
#define PB_OUTPUT(PIN_NAME) (DDRB |= bit(PIN_NAME))    // pinMode(PIN, OUTPUT)
#define PB_INPUT(PIN_NAME) (DDRB &= ~bit(PIN_NAME))    // pinMode(PIN, INPUT)
#define PB_HIGH(PIN_NAME) (PORTB |= bit(PIN_NAME))     // digitalWrite(PIN, 1)
#define PB_LOW(PIN_NAME) (PORTB &= ~bit(PIN_NAME))     // digitalWrite(PIN, 0)
#define PB_CHECK(PIN_NAME) (PORTB & bit(PIN_NAME))     // digitalWrite(PIN, 0)
#define PB_READ(PIN_NAME) (PINB & bit(PIN_NAME))       // digitalRead(PIN)

#define LED_BUTTON                  PB4    // combined led/button pin
#define PIN_SWITCH                  PB1    // pin used for resetting the Raspberry
#define EXT_VOLTAGE                ADC3    // ADC number, used to measure external or RPi voltage (Ax, ADCx or x)

#define BLINK_TIME                  100    // time in milliseconds for the LED to blink
#define RPI_RESTART                 200    // time in milliseconds to pull switch to low to restart RPi
#define NUM_MEASUREMENTS              5    // the number of ADC measurements we average, should be larger than 4

// States
#define RUNNING_STATE        0             // the system is running normally
#define UNCLEAR_STATE        1             // the system has been reset and is unsure about its state
#define REC_WARN_STATE       2             // the system was in the warn state and is now recoveringe
#define REC_SHUTDOWN_STATE   4             // the system was in the shutdown state and is now recovering
#define WARN_STATE           8             // the system was in the warn state
#define SHUTDOWN_STATE      16             // the system was in the shutdown state


// EEPROM address definition
// Base address is used to decide whether data was stored before
// following this is the data
#define EEPROM_BASE_ADDRESS           0    // uint8_t
#define EEPROM_TIMEOUT_ADDRESS        1    // uint8_t
#define EEPROM_PRIMED_ADDRESS         2    // uint8_t
#define EEPROM_FORCE_SHUTDOWN         3    // uint8_t
#define EEPROM_RESTART_V_ADDRESS      4    // uint16_t
#define EEPROM_WARN_V_ADDRESS         6    // uint16_t
#define EEPROM_SHUTDOWN_V_ADDRESS     8    // uint16_t
#define EEPROM_BAT_V_COEFFICIENT     10    // uint16_t
#define EEPROM_BAT_V_CONSTANT        12    // uint16_t
#define EEPROM_EXT_V_COEFFICIENT     14    // uint16_t
#define EEPROM_EXT_V_CONSTANT        16    // uint16_t
#define EEPROM_T_COEFFICIENT         18    // uint16_t
#define EEPROM_T_CONSTANT            20    // uint16_t

#define EEPROM_INIT_VALUE          0x42

// I2C interface definitions
#define I2C_ADDRESS                0x37

#define REGISTER_LAST_ACCESS       0x01
#define REGISTER_BAT_VOLTAGE       0x11
#define REGISTER_EXT_VOLTAGE       0x12
#define REGISTER_BAT_V_COEFFICIENT 0x13
#define REGISTER_BAT_V_CONSTANT    0x14
#define REGISTER_EXT_V_COEFFICIENT 0x15
#define REGISTER_EXT_V_CONSTANT    0x16
#define REGISTER_TIMEOUT           0x21
#define REGISTER_PRIMED            0x22
#define REGISTER_SHOULD_SHUTDOWN   0x23
#define REGISTER_FORCE_SHUTDOWN    0x24
#define REGISTER_RESTART_VOLTAGE   0x31
#define REGISTER_WARN_VOLTAGE      0x32
#define REGISTER_SHUTDOWN_VOLTAGE  0x33
#define REGISTER_TEMPERATURE       0x41
#define REGISTER_T_COEFFICIENT     0x42
#define REGISTER_T_CONSTANT        0x43

#define REGISTER_VERSION           0x80
#define REGISTER_INIT_EEPROM       0xFF

// Shutdown Levels
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
