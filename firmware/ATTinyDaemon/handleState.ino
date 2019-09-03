/*
   The state variable encapsulates the all-over state of the system (ATTiny and RPi
   together).
   The possible states are:
    RUNNING_STATE      -   0 - the system is running normally
    UNCLEAR_STATE      -   1 - the system has been reset and is unsure about its state
    REC_WARN_STATE     -   2 - the system was in the warn state and is now recoveringe
    REC_SHUTDOWN_STATE -   4 - the system was in the shutdown state and is now recovering
    WARN_STATE         -   8 - the system is in the warn state 
    SHUTDOWN_STATE     -  16 - the system is in the shutdown state

    They are ordered in a way that allows to later check for the severity of the state by
    e.g., "if(state < SHUTDOWN_STATE)"

    This function implements the state changes between these states, during the normal
    execution but in the case of a reset as well. For this we have to take into account that
    the only information we might have is the current voltage and we are in the RUNNING_STATE.
*/
void handle_state() {
    // Going down the states is done here, back to running only in the main loop and in handelI2C
  if (bat_voltage <= shutdown_voltage) {
    state = SHUTDOWN_STATE;
  } else if (bat_voltage <= warn_voltage) {
    state = WARN_STATE;
  } else if (bat_voltage <= restart_voltage) {
    if (state == UNCLEAR_STATE && seconds > timeout) {
      // the RPi is not running, even after the timeout, so we assume that it
      // shut down, this means we come from a WARN_STATE or SHUTDOWN_STATE 
      state = WARN_STATE;
    }
  } else { // we are at a safe voltage
    if (state == SHUTDOWN_STATE) {
      state = REC_SHUTDOWN_STATE;
    } else if (state == WARN_STATE) {
      state = REC_WARN_STATE;
    } else if (state == UNCLEAR_STATE) {
      state = RUNNING_STATE;
    }
  }
}
