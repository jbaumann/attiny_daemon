/*
   The state variable encapsulates the all-over state of the system (ATTiny and RPi
   together).
   The possible states are:
    RUNNING_STATE       -  0 - the system is running normally
    UNCLEAR_STATE       -  1 - the system has been reset and is unsure about its state
    WARN_TO_RUNNING     -  2 - the system transitions from warn state to running state
    SHUTDOWN_TO_RUNNING -  4 - the system transitions from shutdown state to running state
    WARN_STATE          -  8 - the system is in the warn state
    WARN_TO_SHUTDOWN    - 16 - the system transitions from warn state to shutdown state
    SHUTDOWN_STATE      - 32 - the system is in the shutdown state

    They are ordered in a way that allows to later check for the severity of the state by
    e.g., "if(state <= WARN_STATE)"

    This function implements the state changes between these states, during the normal
    execution but in the case of a reset as well. For this we have to take into account that
    the only information we might have is the current voltage and we are in the RUNNING_STATE.
*/
void handle_state() {
  if (state <= State::warn_state) {
    if (primed != 0 || (seconds < timeout) ) {
      // start the regular blink if either primed is set or we are not yet in a timeout.
      // This means the LED stops blinking at the same time at which
      // the second button functionality is enabled.
      // We do this here to get additional on-time for the LED during reading the voltages
      ledOn_buttonOff();
    }
  }

  read_voltages();



  
    // Going down the states is done here, back to running only in the main loop and in handelI2C
  if (bat_voltage <= shutdown_voltage) {
    state = State::warn_to_shutdown;
  } else if (bat_voltage <= warn_voltage) {
    state = State::warn_state;
  } else if (bat_voltage <= restart_voltage) {
    if (state == State::unclear_state && seconds > timeout) {
      // the RPi is not running, even after the timeout, so we assume that it
      // shut down, this means we come from a WARN_STATE or SHUTDOWN_STATE 
      state = State::warn_state;
    }
  } else { // we are at a safe voltage
 
    switch(state) {
      case State::shutdown_state: 
        state = State::shutdown_to_running;
        break;
      case State::warn_to_shutdown:
        state = State::shutdown_to_running;
        break;
      case State::warn_state: 
        state = State::warn_to_running;
        break;
      case State::unclear_state: 
        state = State::running_state;
        break;
    }
  }


  
  if (state <= State::warn_state) {
    if (should_shutdown > Shutdown_Cause::rpi_initiated && (seconds < timeout)) {
      // RPi should take action, possibly shut down. Signal by blinking 5 times
        blink_led(5, BLINK_TIME);
    }
  }

  // we act only if primed is set
  if (primed != 0) {
    if(state == State::warn_to_shutdown) {
      // immediately turn off the system if force_shutdown is set
      if (force_shutdown != 0) {
        ups_off();
      }
      state = State::shutdown_state;
    }

    if (state == State::shutdown_state) {
      ledOff_buttonOff();
    } else if (state == State::warn_state) {
      // The RPi has been warned using the should_shutdown variable
      // we simply let it shutdown even if it does not set SL_INITIATED
      reset_counter();
    } else if (state == State::shutdown_to_running) {
      // we have recovered from a shutdown and are now at a safe voltage
      ups_on();
      reset_counter();
      state = State::running_state;
    } else if (state == State::warn_to_running) {
      // we have recovered from a warn state and are now at a safe voltage
      state = State::running_state;
    } else if (state == State::unclear_state) {
      // we do nothing and wait until either a timeout occurs, the voltage
      // drops to warn_voltage or is higher than restart_voltage (see handle_state())
    }

    if (state == State::running_state) {
      if (seconds > timeout) {
        // RPi has not accessed the I2C interface for more than timeout seconds.
        // We restart it. Signal restart by blinking ten times
        blink_led(10, BLINK_TIME / 2);
        restart_raspberry();
        reset_counter();
      }
    }
  }

  if (state <= State::warn_state) {
    // allow the button functionality as long as possible and even if not primed
    ledOff_buttonOn();
  }

}
