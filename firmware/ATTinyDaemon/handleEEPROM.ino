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
  EEPROM.get(EEPROM_RESET_CONFIG, reset_configuration);
  EEPROM.get(EEPROM_RESET_PULSE_LENGTH, reset_pulse_length);
  EEPROM.get(EEPROM_SW_RECOVERY_DELAY, sw_recovery_delay);
  EEPROM.get(EEPROM_LED_OFF_MODE, led_off_mode);
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
  EEPROM.put(EEPROM_RESET_CONFIG, reset_configuration);
  EEPROM.put(EEPROM_RESET_PULSE_LENGTH, reset_pulse_length);
  EEPROM.put(EEPROM_SW_RECOVERY_DELAY, sw_recovery_delay);
  EEPROM.put(EEPROM_LED_OFF_MODE, led_off_mode);

}
