#!/usr/bin/env python3 

import sys

sys.path.append('/opt/attiny_daemon/')  # add the path to our ATTiny module

import time
import smbus
import logging
from attiny_i2c import ATTiny

_time_const = 0.5   # used as a pause between i2c communications, the ATTiny is slow
_num_retries = 10   # the number of retries when reading from or writing to the ATTiny_Daemon
_i2c_address = 0x37 # the I2C address that is used for the ATTiny_Daemon

# set up logging
root_log = logging.getLogger()
root_log.setLevel("INFO")

# set up communication to the ATTiny_Daemon
bus = smbus.SMBus(1)
attiny = ATTiny(bus, _i2c_address, _time_const, _num_retries)

states = {
     0 : "RUNNING_STATE",
     1 : "UNCLEAR_STATE",
     2 : "REC_WARN_STATE",
     4 : "REC_SHUTDOWN_STATE",
     8 : "WARN_STATE",
    16 : "WARN_TO_SHUTDOWN",
    32 : "SHUTDOWN_STATE",
}

state = attiny.get_internal_state()
logging.info("Current state is " + hex(state) + ": " + states[state])

# access data
logging.info("Current battery voltage is " + str(attiny.get_bat_voltage() / 1000) + "V.")
logging.info("Current external voltage is " + str(attiny.get_ext_voltage() / 1000) + "V.")

logging.info("Current warn voltage is " + str(attiny.get_warn_voltage() / 1000) + "V.")
logging.info("Current shutdown voltage is " + str(attiny.get_shutdown_voltage() / 1000) + "V.")
logging.info("Current restart voltage is " + str(attiny.get_restart_voltage() / 1000) + "V.")

