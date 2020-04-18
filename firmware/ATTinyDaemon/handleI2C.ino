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
  if (state == UNCLEAR_STATE) {
    state = RUNNING_STATE;
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
          EEPROM.put(EEPROM_TIMEOUT_ADDRESS, timeout);
          break;
        case REGISTER_PRIMED:
          primed = rbuf[1];
          EEPROM.put(EEPROM_PRIMED_ADDRESS, primed);
          break;
        case REGISTER_SHOULD_SHUTDOWN:
          should_shutdown = rbuf[1];
          break;
        case REGISTER_FORCE_SHUTDOWN:
          force_shutdown = rbuf[1];
          EEPROM.put(EEPROM_FORCE_SHUTDOWN, force_shutdown);
          break;
        case REGISTER_LED_OFF_MODE:
          led_off_mode = rbuf[1];
          EEPROM.put(EEPROM_LED_OFF_MODE, led_off_mode);
          break;          
        case REGISTER_RESET_CONFIG:
          reset_configuration = rbuf[1];
          EEPROM.put(EEPROM_RESET_CONFIG, reset_configuration);
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
          EEPROM.put(EEPROM_RESTART_V_ADDRESS, restart_voltage);
          break;
        case REGISTER_WARN_VOLTAGE:
          warn_voltage = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_WARN_V_ADDRESS, warn_voltage);
          break;
        case REGISTER_SHUTDOWN_VOLTAGE:
          shutdown_voltage = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_SHUTDOWN_V_ADDRESS, shutdown_voltage);
          break;
        case REGISTER_BAT_V_COEFFICIENT:
          bat_v_coefficient = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_BAT_V_COEFFICIENT, bat_v_coefficient);
          break;
        case REGISTER_BAT_V_CONSTANT:
          bat_v_constant = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_BAT_V_CONSTANT, bat_v_constant);
          break;
        case REGISTER_EXT_V_COEFFICIENT:
          ext_v_coefficient = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_EXT_V_COEFFICIENT, ext_v_coefficient);
          break;
        case REGISTER_EXT_V_CONSTANT:
          ext_v_constant = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_EXT_V_CONSTANT, ext_v_constant);
          break;
        case REGISTER_T_COEFFICIENT:
          t_coefficient = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_T_COEFFICIENT, t_coefficient);
          break;
        case REGISTER_T_CONSTANT:
          t_constant = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_T_CONSTANT, t_constant);
          break;
        case REGISTER_RESET_PULSE_LENGTH:
          reset_pulse_length = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_RESET_PULSE_LENGTH, reset_pulse_length);
          break;
        case REGISTER_SW_RECOVERY_DELAY:
          sw_recovery_delay = rbuf[1] | (rbuf[2] << 8);
          EEPROM.put(EEPROM_SW_RECOVERY_DELAY, sw_recovery_delay);
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
      write_data_crc((uint8_t *)&bat_v_coefficient, sizeof(bat_v_coefficient));
      break;
    case REGISTER_BAT_V_CONSTANT:
      write_data_crc((uint8_t *)&bat_v_constant, sizeof(bat_v_constant));
      break;
    case REGISTER_EXT_V_COEFFICIENT:
      write_data_crc((uint8_t *)&ext_v_coefficient, sizeof(ext_v_coefficient));
      break;
    case REGISTER_EXT_V_CONSTANT:
      write_data_crc((uint8_t *)&ext_v_constant, sizeof(ext_v_constant));
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
      write_data_crc((uint8_t *)&t_coefficient, sizeof(t_coefficient));
      break;
    case REGISTER_T_CONSTANT:
      write_data_crc((uint8_t *)&t_constant, sizeof(t_constant));
      break;
    case REGISTER_RESET_CONFIG:
      write_data_crc((uint8_t *)&reset_configuration, sizeof(reset_configuration));
      break;
    case REGISTER_RESET_PULSE_LENGTH:
      write_data_crc((uint8_t *)&reset_pulse_length, sizeof(reset_pulse_length));
      break;      
    case REGISTER_SW_RECOVERY_DELAY:
      write_data_crc((uint8_t *)&sw_recovery_delay, sizeof(sw_recovery_delay));
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
