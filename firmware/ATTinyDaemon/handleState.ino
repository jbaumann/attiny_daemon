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
}
