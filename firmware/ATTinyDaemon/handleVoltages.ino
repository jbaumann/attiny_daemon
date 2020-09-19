/*
   These two macros extract the number of the ADC in question from the value given.
   Format can be either Ax, ADCx or simply x, with x being the number of the ADC.
   So, for ADC3 we can use either A3, ADC3 or 3. We do this by interpreting the given
   value as a hexadecimal number, stripping away everything but the lowest byte.
   This works only with names that are also valid hexadecimal numbers (or which
   evaluate to these, e.g. PB1).
   This is a bit advanced, but the main trick here is to force the preprocessor to first
   substitute the value of the parameter given to ADC_NUMBER before using the string
   concatenator functionality ## by putting this into a macro call of its own. This way,
   the preprocessor first evaluates ADC_NUMBER and before calling _PREPEND__0X, substitutes
   "name" with its contents (see C-Standard ISO/IEC 9899:1999, Ch. 6.10.3.1ff).
*/
#define _PREPEND_0X(val) 0x##val
#define ADC_NUMBER(name) (_PREPEND_0X(name) & 0xF)

/*
   The following code is based on the the ATTiny25/45/85 datasheet
   provided by Microchip. Pages and Chapter numbers are for the
   revision Rev. 2586Q-08/13.

  Table 17-3 states the following for the selection of the reference voltage
  REFS2 REFS1 REFS0   Voltage Reference (VREF) Selection
  X     0     0       VCC used as Voltage Reference, disconnected from PB0 (AREF).
  X     0     1       External Voltage Reference at PB0 (AREF) pin, Internal Voltage Reference turned off.
  0     1     0       Internal 1.1V Voltage Reference.
  0     1     1       Reserved
  1     1     0       Internal 2.56V Voltage Reference without external bypass capacitor, disconnected from PB0 (AREF)(1).
  1     1     1       Internal 2.56V Voltage Reference with external bypass capacitor at PB0 (AREF) pin(1).

   Table 17-4 states for the selection of ADCs:
   MUX[3:0]
   0000  ADC0 (PB5)
   0001  ADC1 (PB2)
   0010  ADC2 (PB4)
   0011  ADC3 (PB3)
   1100  VBG
   1111  ADC4 (Temperature)
*/

void read_voltages() {
  // if we are in shutdown state take only one measurement
  uint8_t num_measurements = state > State::warn_state ? 1 : NUM_MEASUREMENTS; 

  /* Table 17-5 defines the prescaler values. For a clock frequency of 8MHz which we use,
     a divison factor of 64 leads to the needed sample rate of 125kHz, which is in the
     needed 50-200kHz range. For this factor ADPS[2:0] is 110
  */
  //-- Enable ADC with a division factor of 64 -----------------------------------------
  ADCSRA = bit(ADEN) | bit(ADPS2) | bit(ADPS1);

  //-- Measure Temperature -------------------------------------------------------------
  // temperature first because the ADC measurements heat the chip
  // switch to ADC4 and to internal 1.1V reference to measure temperature
  ADMUX = bit(REFS1) | bit(MUX3) | bit(MUX2) | bit(MUX1) | bit(MUX0);

  uint32_t temp_temperature = read_adc(num_measurements);

  uint16_t temperature_coefficient_safe;
  int16_t temperature_constant_safe;
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    temperature_coefficient_safe = temperature_coefficient;
    temperature_constant_safe = temperature_constant;
  }

  temp_temperature *= temperature_coefficient_safe;
  temp_temperature = temp_temperature / 1000 + temperature_constant_safe;

  //-- Measure Vcc ---------------------------------------------------------------------
  /*
    The trick to measure Vcc is to measure the band gap voltage against
    the current Vcc. Since we know that the band gap voltage is very stable
    and around 1.1V we can calculate the current Vcc by "inverting" the result.
  */

  // REFS2, REFS1, REFS0 == 0 selects Vcc as reference, MUX3, MUX2 == 1 selects band gap
  ADMUX = bit(MUX3) | bit(MUX2);

  /*
   Table 17-4 Note2 states:
   After switching to internal voltage reference the ADC requires a settling time
   of 1ms before measurements are stable. Conversions starting before this may not
   be reliable. The ADC must be enabled during the settling time.
  */
  delay(2); // Wait for ADC to settle

  // Calculate Vcc (in mV); 1.126.400 = 1.1*1024*1000, see Ch. 17.11.1 of datasheet
  uint32_t temp_bat_voltage = 1126400L / read_adc(num_measurements);

  // correct the measurement using coefficient and constant
  uint16_t bat_voltage_coefficient_safe;
  int16_t bat_voltage_constant_safe;
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    bat_voltage_coefficient_safe = bat_voltage_coefficient;
    bat_voltage_constant_safe = bat_voltage_constant;
    if(reset_bat_voltage == true) {
      reset_bat_voltage = false;
      bat_voltage = 0;
    }
  }

  temp_bat_voltage *= bat_voltage_coefficient_safe;
  temp_bat_voltage = temp_bat_voltage / 1000 + bat_voltage_constant_safe;


  //-- Measure EXT_V -------------------------------------------------------------------
  // Since the MUX bits are the lowest bits of ADMUX we can simply use the number
  // of the ADC we want to use directly
  ADMUX = ADC_NUMBER(EXT_VOLTAGE);

  uint32_t temp_ext_voltage = read_adc(num_measurements);
  temp_ext_voltage *= temp_bat_voltage;    // normalize relative to Vcc
  temp_ext_voltage /= 1024;

  // correct the measurement using coefficient and constant
  uint16_t ext_voltage_coefficient_safe;
  int16_t ext_voltage_constant_safe;
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    ext_voltage_coefficient_safe = ext_voltage_coefficient;
    ext_voltage_constant_safe = ext_voltage_constant;
  }
  if((signed)temp_ext_voltage > ext_voltage_constant_safe) {
    temp_ext_voltage *= ext_voltage_coefficient_safe;
    temp_ext_voltage = temp_ext_voltage / 1000 + ext_voltage_constant_safe;
  } else {
    temp_ext_voltage = 0;
  }


  //-- Turn off the ADC ----------------------------------------------------------------
  ADCSRA &= ~bit(ADEN); // turn off the ADC

  if (bat_voltage != 0) {
    // Average battery voltage over the last few measurements.
    // This allows us to average out short voltage spikes caused by
    // the Raspberry's different loads.
    temp_bat_voltage = (temp_bat_voltage + bat_voltage * 9) / 10;
  }

  // we use the following block to guarantee that the values are atomically set
  // even in the presence of interrupts from I2C
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    bat_voltage = temp_bat_voltage;
    ext_voltage = temp_ext_voltage;
    temperature = temp_temperature;
  }
}

/*
   This function takes num_measurements ADC measurements, throws away highest and
   lowest and averages the rest. If num_measurements is < 4, we simply average
   all measured values. This allows to get a more precise measurement.
   In addition a the first, extra measurement is always thrown away
*/
uint32_t read_adc(uint8_t num_measurements) {

  uint32_t result = 0;
  uint16_t highest_val = 0;
  uint16_t lowest_val = USHRT_MAX;

  // measure num_measurements + 1 times and throw away first measurement
  for (int i = 0; i <= num_measurements; i++) {
    ADCSRA |= bit(ADSC); // Start conversion
    loop_until_bit_is_clear(ADCSRA, ADSC); // wait for results

    uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
    uint8_t high = ADCH; // unlocks both

    if(i != 0) { // throw away the first measurement
      uint16_t current_val = (high << 8) | low;
      result += current_val;
      if (current_val > highest_val) {
        highest_val = current_val;
      }
      if (current_val < lowest_val) {
        lowest_val = current_val;
      }
    }
  }
  if (num_measurements > 3) {
    result = (result - highest_val - lowest_val) / (num_measurements - 2);
  } else {
    result /= num_measurements;
  }

  return result;  // 32 bit forces correct calculation of voltages in the next step
}
