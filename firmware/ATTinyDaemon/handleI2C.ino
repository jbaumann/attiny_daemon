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
#define BUFFER_SIZE 8
uint8_t rbuf[BUFFER_SIZE];
void receive_event(int bytes) {

  // If we are in an unclear state, then a communication from the RPi moves us to running state
  if(state == UNCLEAR_STATE) {
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
      if (register_number == REGISTER_TIMEOUT) {
        timeout = rbuf[1];
        EEPROM.put(EEPROM_TIMEOUT_ADDRESS, timeout);
      } else if (register_number == REGISTER_PRIMED) {
        primed = rbuf[1];
        EEPROM.put(EEPROM_PRIMED_ADDRESS, primed);
      } else if (register_number == REGISTER_SHOULD_SHUTDOWN) {
        should_shutdown = rbuf[1];
      } else if (register_number == REGISTER_FORCE_SHUTDOWN) {
        force_shutdown = rbuf[1];
        EEPROM.put(EEPROM_FORCE_SHUTDOWN, force_shutdown);
      } else if (register_number == REGISTER_INIT_EEPROM) {
        uint8_t init_eeprom = rbuf[1];

        if (init_eeprom != 0) {
          init_EEPROM();
        }
      }
    } else if (bytes == 4) {
      // write a 16 bit register
      if (register_number == REGISTER_RESTART_VOLTAGE) {
        restart_voltage = rbuf[1] | (rbuf[2] << 8);
        EEPROM.put(EEPROM_RESTART_V_ADDRESS, restart_voltage);
      } else if (register_number == REGISTER_WARN_VOLTAGE) {
        warn_voltage = rbuf[1] | (rbuf[2] << 8);
        EEPROM.put(EEPROM_WARN_V_ADDRESS, warn_voltage);
      } else if (register_number == REGISTER_SHUTDOWN_VOLTAGE) {
        shutdown_voltage = rbuf[1] | (rbuf[2] << 8);
        EEPROM.put(EEPROM_SHUTDOWN_V_ADDRESS, shutdown_voltage);
      } else if (register_number == REGISTER_BAT_V_COEFFICIENT) {
        bat_v_coefficient = rbuf[1] | (rbuf[2] << 8);
        EEPROM.put(EEPROM_BAT_V_COEFFICIENT, bat_v_coefficient);
      } else if (register_number == REGISTER_BAT_V_CONSTANT) {
        bat_v_constant = rbuf[1] | (rbuf[2] << 8);
        EEPROM.put(EEPROM_BAT_V_CONSTANT, bat_v_constant);
      } else if (register_number == REGISTER_EXT_V_COEFFICIENT) {
        ext_v_coefficient = rbuf[1] | (rbuf[2] << 8);
        EEPROM.put(EEPROM_EXT_V_COEFFICIENT, ext_v_coefficient);
      } else if (register_number == REGISTER_EXT_V_CONSTANT) {
        ext_v_constant = rbuf[1] | (rbuf[2] << 8);
        EEPROM.put(EEPROM_EXT_V_CONSTANT, ext_v_constant);
      } else if (register_number == REGISTER_T_COEFFICIENT) {
        t_coefficient = rbuf[1] | (rbuf[2] << 8);
        EEPROM.put(EEPROM_T_COEFFICIENT, t_coefficient);
      } else if (register_number == REGISTER_T_CONSTANT) {
        t_constant = rbuf[1] | (rbuf[2] << 8);
        EEPROM.put(EEPROM_T_CONSTANT, t_constant);
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
    A switch construct might be better readable but needs
    36 bytes more if we use break statements, and still 30
    bytes more without break statements.
  */

  if (register_number == REGISTER_LAST_ACCESS) {
    //Wire.write((uint8_t *)&seconds, sizeof(seconds));
    write_data_crc((uint8_t *)&seconds, sizeof(seconds));

  } else if (register_number == REGISTER_BAT_VOLTAGE) {
    //Wire.write((uint8_t *)&bat_voltage, sizeof(bat_voltage));
    write_data_crc((uint8_t *)&bat_voltage, sizeof(bat_voltage));    

  } else if (register_number == REGISTER_EXT_VOLTAGE) {
    //Wire.write((uint8_t *)&ext_voltage, sizeof(ext_voltage));
    write_data_crc((uint8_t *)&ext_voltage, sizeof(ext_voltage));

  } else if (register_number == REGISTER_BAT_V_COEFFICIENT) {
    //Wire.write((uint8_t *)&bat_v_coefficient, sizeof(bat_v_coefficient));
    write_data_crc((uint8_t *)&bat_v_coefficient, sizeof(bat_v_coefficient));

  } else if (register_number == REGISTER_BAT_V_CONSTANT) {
    //Wire.write((uint8_t *)&bat_v_constant, sizeof(bat_v_constant));
    write_data_crc((uint8_t *)&bat_v_constant, sizeof(bat_v_constant));

  } else if (register_number == REGISTER_EXT_V_COEFFICIENT) {
    //Wire.write((uint8_t *)&ext_v_coefficient, sizeof(ext_v_coefficient));
    write_data_crc((uint8_t *)&ext_v_coefficient, sizeof(ext_v_coefficient));

  } else if (register_number == REGISTER_EXT_V_CONSTANT) {
    //Wire.write((uint8_t *)&ext_v_constant, sizeof(ext_v_constant));
    write_data_crc((uint8_t *)&ext_v_constant, sizeof(ext_v_constant));

  } else if (register_number == REGISTER_TIMEOUT) {
    //Wire.write((uint8_t *)&timeout, sizeof(timeout));
    write_data_crc((uint8_t *)&timeout, sizeof(timeout));

  } else if (register_number == REGISTER_PRIMED) {
    //Wire.write((uint8_t *)&primed, sizeof(primed));
    write_data_crc((uint8_t *)&primed, sizeof(primed));

  } else if (register_number == REGISTER_SHOULD_SHUTDOWN) {
    //Wire.write((uint8_t *)&should_shutdown, sizeof(should_shutdown));
    write_data_crc((uint8_t *)&should_shutdown, sizeof(should_shutdown));

  } else if (register_number == REGISTER_FORCE_SHUTDOWN) {
    //Wire.write((uint8_t *)&force_shutdown, sizeof(force_shutdown));
    write_data_crc((uint8_t *)&force_shutdown, sizeof(force_shutdown));

  } else if (register_number == REGISTER_RESTART_VOLTAGE) {
    //Wire.write((uint8_t *)&restart_voltage, sizeof(restart_voltage));
    write_data_crc((uint8_t *)&restart_voltage, sizeof(restart_voltage));

  } else if (register_number == REGISTER_WARN_VOLTAGE) {
    //Wire.write((uint8_t *)&warn_voltage, sizeof(warn_voltage));
    write_data_crc((uint8_t *)&warn_voltage, sizeof(warn_voltage));

  } else if (register_number == REGISTER_SHUTDOWN_VOLTAGE) {
    //Wire.write((uint8_t *)&shutdown_voltage, sizeof(shutdown_voltage));
    write_data_crc((uint8_t *)&shutdown_voltage, sizeof(shutdown_voltage));

  } else if (register_number == REGISTER_TEMPERATURE) {
    //Wire.write((uint8_t *)&temperature, sizeof(temperature));
    write_data_crc((uint8_t *)&temperature, sizeof(temperature));

  } else if (register_number == REGISTER_T_COEFFICIENT) {
    //Wire.write((uint8_t *)&t_coefficient, sizeof(t_coefficient));
    write_data_crc((uint8_t *)&t_coefficient, sizeof(t_coefficient));

  } else if (register_number == REGISTER_T_CONSTANT) {
    //Wire.write((uint8_t *)&t_constant, sizeof(t_constant));
    write_data_crc((uint8_t *)&t_constant, sizeof(t_constant));

  } else if (register_number == REGISTER_VERSION) {
    //Wire.write((uint8_t *)&prog_version, sizeof(prog_version));
    write_data_crc((uint8_t *)&prog_version, sizeof(prog_version));

  }


  //Wire.write(&register_number, 1);
  // we had a read operation and reset the counter
  reset_counter();
}
