#include <USIWire.h>
#include <util/atomic.h>
#include <EEPROM.h>
#include <limits.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

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

#define LED_BUTTON                  PB4    // combined led/button pin
#define PIN_SWITCH                  PB1    // pin used for resetting the Raspberry
#define EXT_VOLTAGE                ADC3    // ADC number, used to measure external or RPi voltage (Ax, ADCx or x)

#define BLINK_TIME                  100    // time in milliseconds for the LED to blink
#define RPI_RESTART                 200    // time in milliseconds to pull switch to low to restart RPi
#define NUM_MEASUREMENTS              5    // the number of ADC measurements we average, should be larger than 4

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

#define REGISTER_INIT_EEPROM       0xFF

// Shutdown Levels
#define SL_NORMAL                bit(0)
#define SL_INITIATED             bit(1)
#define SL_EXT_V                 bit(2)
#define SL_BUTTON                bit(3)
#define SL_BAT_V                 bit(7)
