/*
   This method is called when either a register number is transferred (1 byte)
   or data is written to a register.
   When data is written we use a simple protocol to guarantee that the data has
   been received correctly. The last byte transmitted by the sender has to repeat
   the register value, i.e. first byte == last byte (rbuf[0] == rbuf[-1]).
   When data is requested we simply send the data on the bus and hope for the best.
   Transmission errors are fixed on the receiving side (the Raspberry) by simply
   retrying the read.
*/
const uint8_t BUFFER_SIZE = 8;

uint8_t rbuf[BUFFER_SIZE];
void receive_event(int bytes) {

  // If we are in an unclear state, then a communication from the RPi moves us to running state
  if (state == State::unclear_state) {
    state = State::running_state;
  }

  uint8_t count = BUFFER_SIZE > bytes ? bytes : BUFFER_SIZE;
  for (int i = 0; i < count; i++) {
    rbuf[i] = Wire.read();
  }
  if (bytes > BUFFER_SIZE) {
    // something is seriously wrong. Clean up and try to recover
    for (int i = BUFFER_SIZE; i < bytes; i++)
      Wire.read();
  }

  // Read the first byte to determine which register is concerned
  register_number = rbuf[0];

  // check that the data has been received correctly
  uint8_t crc = crc8_message_calc(rbuf, bytes - 1);
  if (crc == rbuf[bytes - 1])
  {
    // If there is more than 1 byte, then the master is writing to the slave
    if (bytes == 3) {
      // write an 8 bit register
      switch (register_number) {
        case REGISTER_TIMEOUT:
          timeout = rbuf[1];
          EEPROM.put(EEPROM_Address::timeout, timeout);
          break;
        case REGISTER_PRIMED:
          primed = rbuf[1];
          EEPROM.put(EEPROM_Address::primed, primed);
          break;
        case REGISTER_SHOULD_SHUTDOWN:
          should_shutdown = rbuf[1];
          break;
        case REGISTER_FORCE_SHUTDOWN:
          force_shutdown = rbuf[1];
          EEPROM.put(EEPROM_Address::force_shutdown, force_shutdown);
          break;
        case REGISTER_LED_OFF_MODE:
          led_off_mode = rbuf[1];
          EEPROM.put(EEPROM_Address::led_off_mode, led_off_mode);
          break;          
        case REGISTER_RESET_CONFIG:
          reset_configuration = rbuf[1];
          EEPROM.put(EEPROM_Address::reset_configuration, reset_configuration);
          break;
        case REGISTER_INIT_EEPROM:
          uint8_t init_eeprom = rbuf[1];

          if (init_eeprom != 0) {
            init_EEPROM();
          }
          break;
      }


    } else if (bytes == 4) {
      // write a 16 bit register

      switch (register_number) {
        case REGISTER_RESTART_VOLTAGE:
          restart_voltage = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::restart_voltage, restart_voltage);
          break;
        case REGISTER_WARN_VOLTAGE:
          warn_voltage = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::warn_voltage, warn_voltage);
          break;
        case REGISTER_SHUTDOWN_VOLTAGE:
          shutdown_voltage = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::shutdown_voltage, shutdown_voltage);
          break;
        case REGISTER_BAT_V_COEFFICIENT:
          bat_voltage_coefficient = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::bat_voltage_coefficient, bat_voltage_coefficient);
          break;
        case REGISTER_BAT_V_CONSTANT:
          bat_voltage_constant = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::bat_voltage_constant, bat_voltage_constant);
          break;
        case REGISTER_EXT_V_COEFFICIENT:
          ext_voltage_coefficient = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::ext_voltage_coefficient, ext_voltage_coefficient);
          break;
        case REGISTER_EXT_V_CONSTANT:
          ext_voltage_constant = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::ext_voltage_constant, ext_voltage_constant);
          break;
        case REGISTER_T_COEFFICIENT:
          temperature_coefficient = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::temperature_coefficient, temperature_coefficient);
          break;
        case REGISTER_T_CONSTANT:
          temperature_constant = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::temperature_constant, temperature_constant);
          break;
        case REGISTER_RESET_PULSE_LENGTH:
          reset_pulse_length = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::reset_pulse_length, reset_pulse_length);
          break;
        case REGISTER_SW_RECOVERY_DELAY:
          switch_recovery_delay = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_Address::switch_recovery_delay, switch_recovery_delay);
          break;
      }
    }
  }
  if (bytes != 1) {
    // we had a write operation and reset the counter
    reset_counter();
  }
}

/*
   This method is called after receiveEvent() if the master wants to
   read data. The register_number contains the register to read.
*/
void request_event() {
  /*
    Read from the register variable to know what to send back.
  */
  switch (register_number) {

    case REGISTER_LAST_ACCESS:
      write_data_crc((uint8_t *)&seconds, sizeof(seconds));
      break;
    case REGISTER_BAT_VOLTAGE:
      write_data_crc((uint8_t *)&bat_voltage, sizeof(bat_voltage));
      break;
    case REGISTER_EXT_VOLTAGE:
      write_data_crc((uint8_t *)&ext_voltage, sizeof(ext_voltage));
      break;
    case REGISTER_BAT_V_COEFFICIENT:
      write_data_crc((uint8_t *)&bat_voltage_coefficient, sizeof(bat_voltage_coefficient));
      break;
    case REGISTER_BAT_V_CONSTANT:
      write_data_crc((uint8_t *)&bat_voltage_constant, sizeof(bat_voltage_constant));
      break;
    case REGISTER_EXT_V_COEFFICIENT:
      write_data_crc((uint8_t *)&ext_voltage_coefficient, sizeof(ext_voltage_coefficient));
      break;
    case REGISTER_EXT_V_CONSTANT:
      write_data_crc((uint8_t *)&ext_voltage_constant, sizeof(ext_voltage_constant));
      break;
    case REGISTER_TIMEOUT:
      write_data_crc((uint8_t *)&timeout, sizeof(timeout));
      break;
    case REGISTER_PRIMED:
      write_data_crc((uint8_t *)&primed, sizeof(primed));
      break;
    case REGISTER_SHOULD_SHUTDOWN:
      write_data_crc((uint8_t *)&should_shutdown, sizeof(should_shutdown));
      break;
    case REGISTER_FORCE_SHUTDOWN:
      write_data_crc((uint8_t *)&force_shutdown, sizeof(force_shutdown));
      break;
    case REGISTER_LED_OFF_MODE:
      write_data_crc((uint8_t *)&led_off_mode, sizeof(led_off_mode));
      break;      
    case REGISTER_RESTART_VOLTAGE:
      write_data_crc((uint8_t *)&restart_voltage, sizeof(restart_voltage));
      break;
    case REGISTER_WARN_VOLTAGE:
      write_data_crc((uint8_t *)&warn_voltage, sizeof(warn_voltage));
      break;
    case REGISTER_SHUTDOWN_VOLTAGE:
      write_data_crc((uint8_t *)&shutdown_voltage, sizeof(shutdown_voltage));
      break;
    case REGISTER_TEMPERATURE:
      write_data_crc((uint8_t *)&temperature, sizeof(temperature));
      break;
    case REGISTER_T_COEFFICIENT:
      write_data_crc((uint8_t *)&temperature_coefficient, sizeof(temperature_coefficient));
      break;
    case REGISTER_T_CONSTANT:
      write_data_crc((uint8_t *)&temperature_constant, sizeof(temperature_constant));
      break;
    case REGISTER_RESET_CONFIG:
      write_data_crc((uint8_t *)&reset_configuration, sizeof(reset_configuration));
      break;
    case REGISTER_RESET_PULSE_LENGTH:
      write_data_crc((uint8_t *)&reset_pulse_length, sizeof(reset_pulse_length));
      break;      
    case REGISTER_SW_RECOVERY_DELAY:
      write_data_crc((uint8_t *)&switch_recovery_delay, sizeof(switch_recovery_delay));
      break;
    case REGISTER_VERSION:
      write_data_crc((uint8_t *)&prog_version, sizeof(prog_version));
      break;
    case REGISTER_FUSE_LOW:
      write_data_crc((uint8_t *)&fuse_low, sizeof(fuse_low));
      break;
    case REGISTER_FUSE_HIGH:
      write_data_crc((uint8_t *)&fuse_high, sizeof(fuse_high));
      break;
    case REGISTER_FUSE_EXTENDED:
      write_data_crc((uint8_t *)&fuse_extended, sizeof(fuse_extended));
      break;
    case REGISTER_INTERNAL_STATE:
      write_data_crc((uint8_t *)&state, sizeof(state));
      break;
  }


  // we had a read operation and reset the counter
  reset_counter();
}
