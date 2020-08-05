/*
   EEPROM class that works with volatile values. We add atomic access
   to guarantee that only valid data is written to and read from 
   the EEPROM.
 */
struct MyEEPROMClass : EEPROMClass {
    template< typename T > T &get( int idx, volatile T &t ){
      T tmp;
      EEPROMClass::get(idx, tmp);
      ATOMIC_BLOCK(ATOMIC_FORCEON) {
        t = tmp;
      }      
    };
    template< typename T > const T &put( int idx, volatile T &t ){
      T tmp;
      ATOMIC_BLOCK(ATOMIC_FORCEON) {
        tmp = t;
      }
      EEPROMClass::put(idx, tmp);     
    };
};
static MyEEPROMClass MyEEPROM;




/*
   Read the EEPROM if it contains valid data, otherwise initialize it.
 */

void  read_or_init_EEPROM() {
  uint8_t writtenBefore;
  EEPROM.get(EEPROM_Address::base, writtenBefore);
  if (writtenBefore != EEPROM_INIT_VALUE) {
    // no data has been written before, initialise EEPROM
    write_EEPROM();
  } else {
    read_EEPROM_values();
  }
}

/*
   Read the values stored in the EEPROM. The addresses are defined in
   the header file. We use the modern get()-method that determines the
   object size itself, because the accompanying put()-method uses the
   update()-function that checks whether the data has been modified
   before it writes.
*/
void read_EEPROM_values() {
  MyEEPROM.get(EEPROM_Address::timeout, timeout);
  MyEEPROM.get(EEPROM_Address::primed, primed);
  MyEEPROM.get(EEPROM_Address::force_shutdown, force_shutdown);  
  MyEEPROM.get(EEPROM_Address::restart_voltage, restart_voltage);
  MyEEPROM.get(EEPROM_Address::warn_voltage, warn_voltage);
  MyEEPROM.get(EEPROM_Address::shutdown_voltage, shutdown_voltage);
  MyEEPROM.get(EEPROM_Address::bat_voltage_coefficient, bat_voltage_coefficient);
  MyEEPROM.get(EEPROM_Address::bat_voltage_constant, bat_voltage_constant);
  MyEEPROM.get(EEPROM_Address::ext_voltage_coefficient, ext_voltage_coefficient);
  MyEEPROM.get(EEPROM_Address::ext_voltage_constant, ext_voltage_constant);
  MyEEPROM.get(EEPROM_Address::temperature_coefficient, temperature_coefficient);
  MyEEPROM.get(EEPROM_Address::temperature_constant, temperature_constant);
  MyEEPROM.get(EEPROM_Address::reset_configuration, reset_configuration);
  MyEEPROM.get(EEPROM_Address::reset_pulse_length, reset_pulse_length);
  MyEEPROM.get(EEPROM_Address::switch_recovery_delay, switch_recovery_delay);
  MyEEPROM.get(EEPROM_Address::led_off_mode, led_off_mode);
  MyEEPROM.get(EEPROM_Address::vext_off_is_shutdown, vext_off_is_shutdown);
}

/*
   Writes the EEPROM and set the values to the currently
   held values in our variables. This function is called when,
   in the setup() function, we determine that no valid EEPROM
   data can be read (by checking the EEPROM_INIT_VALUE).
   This method can also be used later from the Raspberry to
   update or reinit the EEPROM.
*/
void write_EEPROM() {
  // put uses update(), thus no unnecessary writes
  EEPROM.put(EEPROM_Address::base, EEPROM_INIT_VALUE);
  MyEEPROM.put(EEPROM_Address::timeout, timeout);
  MyEEPROM.put(EEPROM_Address::primed, primed);
  MyEEPROM.put(EEPROM_Address::force_shutdown, force_shutdown);
  MyEEPROM.put(EEPROM_Address::restart_voltage, restart_voltage);
  MyEEPROM.put(EEPROM_Address::warn_voltage, warn_voltage);
  MyEEPROM.put(EEPROM_Address::shutdown_voltage, shutdown_voltage);
  MyEEPROM.put(EEPROM_Address::bat_voltage_coefficient, bat_voltage_coefficient);
  MyEEPROM.put(EEPROM_Address::bat_voltage_constant, bat_voltage_constant);
  MyEEPROM.put(EEPROM_Address::ext_voltage_coefficient, ext_voltage_coefficient);
  MyEEPROM.put(EEPROM_Address::ext_voltage_constant, ext_voltage_constant);
  MyEEPROM.put(EEPROM_Address::temperature_coefficient, temperature_coefficient);
  MyEEPROM.put(EEPROM_Address::temperature_constant, temperature_constant);
  MyEEPROM.put(EEPROM_Address::reset_configuration, reset_configuration);
  MyEEPROM.put(EEPROM_Address::reset_pulse_length, reset_pulse_length);
  MyEEPROM.put(EEPROM_Address::switch_recovery_delay, switch_recovery_delay);
  MyEEPROM.put(EEPROM_Address::led_off_mode, led_off_mode);
  MyEEPROM.put(EEPROM_Address::vext_off_is_shutdown, vext_off_is_shutdown);
}
