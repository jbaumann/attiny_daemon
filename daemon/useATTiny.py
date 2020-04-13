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

# access data
(major, minor, patch) = attiny.get_version()
version = str(major) + "." + str(minor) + "." + str(patch)
logging.info("Current Version is " + version)

logging.info("Current temperature is " + str(attiny.get_temperature()) + " degrees Celsius.")

logging.info("Current battery voltage is " + str(attiny.get_bat_voltage() / 1000) + "V.")
logging.info("Current external voltage is " + str(attiny.get_ext_voltage() / 1000) + "V.")

logging.info("Current timeout is " + str(attiny.get_timeout()))
logging.info("Current primed is " + str(attiny.get_primed()))
logging.info("Current force_shutdown is " + str(attiny.get_force_shutdown()))

logging.info("Current warn voltage is " + str(attiny.get_warn_voltage() / 1000) + "V.")
logging.info("Current shutdown voltage is " + str(attiny.get_shutdown_voltage() / 1000) + "V.")
logging.info("Current restart voltage is " + str(attiny.get_restart_voltage() / 1000) + "V.")

logging.info("Current reset configuration is " + str(attiny.get_reset_configuration()))
logging.info("Current reset pulse length is " + str(attiny.get_reset_pulse_length()))
logging.info("Current switch recovery delay is " + str(attiny.get_switch_recovery_delay()))


logging.info("Low fuse is " + hex(attiny.get_fuse_low()))
logging.info("High fuse is " + hex(attiny.get_fuse_high()))
logging.info("Extended fuse is " + hex(attiny.get_fuse_extended()))
