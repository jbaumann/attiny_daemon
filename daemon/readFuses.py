#!/usr/bin/env python3 

import sys

sys.path.append('/opt/attiny_daemon/')  # add the path to our ATTiny module

import time
import smbus
import logging
from attiny_i2c import ATTiny

_time_const = 0.7   # used as a pause between i2c communications, the ATTiny is slow
_num_retries = 10   # the number of retries when reading from or writing to the ATTiny_Daemon
_i2c_address = 0x37 # the I2C address that is used for the ATTiny_Daemon

# set up logging
root_log = logging.getLogger()
root_log.setLevel("INFO")

# set up communication to the ATTiny_Daemon
bus = smbus.SMBus(1)
attiny = ATTiny(bus, _i2c_address, _time_const, _num_retries)

# access data
logging.info("Low fuse is " + hex(attiny.get_fuse_low()))
logging.info("High fuse is " + hex(attiny.get_fuse_high()))
logging.info("Extended fuse is " + hex(attiny.get_fuse_extended()))
