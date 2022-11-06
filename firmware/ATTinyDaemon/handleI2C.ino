/*
   Initialize the I2C connection
 */
void init_I2C() {
#if defined SERIAL_DEBUG
  Serial.println(F("In init_I2C()"));
#endif
  Wire.end();
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(request_event);
  Wire.onReceive(receive_event);
}

/*
  The buffer used for holding the data communicated via I2C.
  This doesn't need to be volatile because it is only accessed
  during the interrupt.
 */
const uint8_t BUFFER_SIZE = 8;
uint8_t rbuf[BUFFER_SIZE];

/*
  This variable holds the register for the I2C communication
  This doesn't need to be volatile because it is only accessed
  during the interrupt.
*/
Register register_number;

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

void receive_event(int bytes) {

  disable_watchdog();
  
  i2c_triggered_state_change();

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
  register_number = static_cast<Register>(rbuf[0]);

  // check that the data has been received correctly
  uint8_t crc = crc8_message_calc(rbuf, bytes - 1);
  if (crc == rbuf[bytes - 1])
  {
    // If there is more than 1 byte, then the master is writing to the slave
    if (bytes == 3) {
      // turn off warnings for unhandled enumeration values
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wswitch"

      // write an 8 bit register
      switch (register_number) {
        case Register::timeout:
          timeout = rbuf[1];
          update_eeprom = true;
          break;
        case Register::primed:
          primed = rbuf[1];
          update_eeprom = true;
          break;
        case Register::should_shutdown:
          // normally simply bit-or the info from the RPi, but allow 0 to reset all conditions
          if(rbuf[1] == 0) {
            should_shutdown = 0;
          } else {
            should_shutdown |= rbuf[1];
          }
          break;
        case Register::force_shutdown:
          force_shutdown = rbuf[1];
          update_eeprom = true;
          break;
        case Register::led_off_mode:
          led_off_mode = rbuf[1];
          update_eeprom = true;
          break;
        case Register::ups_configuration:
          ups_configuration = rbuf[1];
          update_eeprom = true;
          break;
        case Register::vext_off_is_shutdown:
          vext_off_is_shutdown = rbuf[1];
          if(vext_off_is_shutdown) {
            // we have to check the external voltage when depending on its value
            ups_configuration |= UPS_Configuration::Value::check_ext_voltage;
          }
          update_eeprom = true;
          break;
        case Register::init_eeprom:
        {
          uint8_t init_eeprom = rbuf[1];

          if (init_eeprom != 0) {
            update_eeprom = true;
          }
          break;
        }
        default:
          break;
      }
    #pragma GCC diagnostic pop


    } else if (bytes == 4) {
      // turn off warnings for unhandled enumeration values
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wswitch"

      // write a 16 bit register
      switch (register_number) {
        case Register::restart_voltage:
          restart_voltage = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::warn_voltage:
          warn_voltage = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::ups_shutdown_voltage:
          ups_shutdown_voltage = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::bat_voltage_coefficient:
          bat_voltage_coefficient = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          reset_bat_voltage = true;  // reset bat_voltage average
          break;
        case Register::bat_voltage_constant:
          bat_voltage_constant = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          reset_bat_voltage = true;  // reset bat_voltage average
          break;
        case Register::ext_voltage_coefficient:
          ext_voltage_coefficient = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::ext_voltage_constant:
          ext_voltage_constant = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::temperature_coefficient:
          temperature_coefficient = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::temperature_constant:
          temperature_constant = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::pulse_length:
          pulse_length = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::pulse_length_on:
          pulse_length_on = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::pulse_length_off:
          pulse_length_off = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        case Register::switch_recovery_delay:
          switch_recovery_delay = rbuf[1] | (rbuf[2] << 8);
          update_eeprom = true;
          break;
        default:
          break;
      }
    #pragma GCC diagnostic pop
    }
  }
  if (bytes != 1) {
    // we had a write operation and reset the counter
    reset_counter_Int();
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
  // turn off warnings for unhandled enumeration values
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wswitch"

  switch (register_number) {

    case Register::last_access:
      write_data_crc((uint8_t *)&seconds, sizeof(seconds));
      break;
    case Register::bat_voltage:
      write_data_crc((uint8_t *)&bat_voltage, sizeof(bat_voltage));
      break;
    case Register::ext_voltage:
      write_data_crc((uint8_t *)&ext_voltage, sizeof(ext_voltage));
      break;
    case Register::bat_voltage_coefficient:
      write_data_crc((uint8_t *)&bat_voltage_coefficient, sizeof(bat_voltage_coefficient));
      break;
    case Register::bat_voltage_constant:
      write_data_crc((uint8_t *)&bat_voltage_constant, sizeof(bat_voltage_constant));
      break;
    case Register::ext_voltage_coefficient:
      write_data_crc((uint8_t *)&ext_voltage_coefficient, sizeof(ext_voltage_coefficient));
      break;
    case Register::ext_voltage_constant:
      write_data_crc((uint8_t *)&ext_voltage_constant, sizeof(ext_voltage_constant));
      break;
    case Register::timeout:
      write_data_crc((uint8_t *)&timeout, sizeof(timeout));
      break;
    case Register::primed:
      write_data_crc((uint8_t *)&primed, sizeof(primed));
      break;
    case Register::should_shutdown:
      write_data_crc((uint8_t *)&should_shutdown, sizeof(should_shutdown));
      break;
    case Register::force_shutdown:
      write_data_crc((uint8_t *)&force_shutdown, sizeof(force_shutdown));
      break;
    case Register::led_off_mode:
      write_data_crc((uint8_t *)&led_off_mode, sizeof(led_off_mode));
      break;      
    case Register::vext_off_is_shutdown:
      write_data_crc((uint8_t *)&vext_off_is_shutdown, sizeof(vext_off_is_shutdown));
      break;      
    case Register::restart_voltage:
      write_data_crc((uint8_t *)&restart_voltage, sizeof(restart_voltage));
      break;
    case Register::warn_voltage:
      write_data_crc((uint8_t *)&warn_voltage, sizeof(warn_voltage));
      break;
    case Register::ups_shutdown_voltage:
      write_data_crc((uint8_t *)&ups_shutdown_voltage, sizeof(ups_shutdown_voltage));
      break;
    case Register::temperature:
      write_data_crc((uint8_t *)&temperature, sizeof(temperature));
      break;
    case Register::temperature_coefficient:
      write_data_crc((uint8_t *)&temperature_coefficient, sizeof(temperature_coefficient));
      break;
    case Register::temperature_constant:
      write_data_crc((uint8_t *)&temperature_constant, sizeof(temperature_constant));
      break;
    case Register::ups_configuration:
      write_data_crc((uint8_t *)&ups_configuration, sizeof(ups_configuration));
      break;
    case Register::pulse_length:
      write_data_crc((uint8_t *)&pulse_length, sizeof(pulse_length));
      break;
    case Register::pulse_length_on:
      write_data_crc((uint8_t *)&pulse_length_on, sizeof(pulse_length_on));
      break;
    case Register::pulse_length_off:
      write_data_crc((uint8_t *)&pulse_length_off, sizeof(pulse_length_off));
      break;
    case Register::switch_recovery_delay:
      write_data_crc((uint8_t *)&switch_recovery_delay, sizeof(switch_recovery_delay));
      break;
    case Register::version:
      write_data_crc((uint8_t *)&prog_version, sizeof(prog_version));
      break;
    case Register::fuse_low:
      write_data_crc((uint8_t *)&fuse_low, sizeof(fuse_low));
      break;
    case Register::fuse_high:
      write_data_crc((uint8_t *)&fuse_high, sizeof(fuse_high));
      break;
    case Register::fuse_extended:
      write_data_crc((uint8_t *)&fuse_extended, sizeof(fuse_extended));
      break;
    case Register::internal_state:
      write_data_crc((uint8_t *)&state, sizeof(state));
      break;
    case Register::uptime: {
      // without curly braces gcc produces faulty code... finding this took a long time
      uint32_t uptime = millis();
      write_data_crc((uint8_t *)&uptime, sizeof(uptime));
      break;
    }
    case Register::mcu_status_register:
      write_data_crc((uint8_t *)&mcusr_mirror, sizeof(mcusr_mirror));
      break;
    default:
      break;
  }
  #pragma GCC diagnostic pop

  // we had a read operation and reset the counter
  reset_counter_Int();
}
