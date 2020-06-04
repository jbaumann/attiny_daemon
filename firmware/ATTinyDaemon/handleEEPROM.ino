/*
   Read the EEPROM if it contains valid data, otherwise initialize it.
 */

void  read_or_init_EEPROM() {
  uint8_t writtenBefore;
  EEPROM.get(EEPROM_Address::base, writtenBefore);
  if (writtenBefore != EEPROM_INIT_VALUE) {
    // no data has been written before, initialise EEPROM
    init_EEPROM();
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
  EEPROM.get(EEPROM_Address::timeout, timeout);
  EEPROM.get(EEPROM_Address::primed, primed);
  EEPROM.get(EEPROM_Address::force_shutdown, force_shutdown);  
  EEPROM.get(EEPROM_Address::restart_voltage, restart_voltage);
  EEPROM.get(EEPROM_Address::warn_voltage, warn_voltage);
  EEPROM.get(EEPROM_Address::shutdown_voltage, shutdown_voltage);
  EEPROM.get(EEPROM_Address::bat_voltage_coefficient, bat_voltage_coefficient);
  EEPROM.get(EEPROM_Address::bat_voltage_constant, bat_voltage_constant);
  EEPROM.get(EEPROM_Address::ext_voltage_coefficient, ext_voltage_coefficient);
  EEPROM.get(EEPROM_Address::ext_voltage_constant, ext_voltage_constant);
  EEPROM.get(EEPROM_Address::temperature_coefficient, temperature_coefficient);
  EEPROM.get(EEPROM_Address::temperature_constant, temperature_constant);
  EEPROM.get(EEPROM_Address::reset_configuration, reset_configuration);
  EEPROM.get(EEPROM_Address::reset_pulse_length, reset_pulse_length);
  EEPROM.get(EEPROM_Address::switch_recovery_delay, switch_recovery_delay);
  EEPROM.get(EEPROM_Address::led_off_mode, led_off_mode);
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
  EEPROM.put(EEPROM_Address::base, EEPROM_INIT_VALUE);
  EEPROM.put(EEPROM_Address::timeout, timeout);
  EEPROM.put(EEPROM_Address::primed, primed);
  EEPROM.put(EEPROM_Address::force_shutdown, force_shutdown);
  EEPROM.put(EEPROM_Address::restart_voltage, restart_voltage);
  EEPROM.put(EEPROM_Address::warn_voltage, warn_voltage);
  EEPROM.put(EEPROM_Address::shutdown_voltage, shutdown_voltage);
  EEPROM.put(EEPROM_Address::bat_voltage_coefficient, bat_voltage_coefficient);
  EEPROM.put(EEPROM_Address::bat_voltage_constant, bat_voltage_constant);
  EEPROM.put(EEPROM_Address::ext_voltage_coefficient, ext_voltage_coefficient);
  EEPROM.put(EEPROM_Address::ext_voltage_constant, ext_voltage_constant);
  EEPROM.put(EEPROM_Address::temperature_coefficient, temperature_coefficient);
  EEPROM.put(EEPROM_Address::temperature_constant, temperature_constant);
  EEPROM.put(EEPROM_Address::reset_configuration, reset_configuration);
  EEPROM.put(EEPROM_Address::reset_pulse_length, reset_pulse_length);
  EEPROM.put(EEPROM_Address::switch_recovery_delay, switch_recovery_delay);
  EEPROM.put(EEPROM_Address::led_off_mode, led_off_mode);
}
