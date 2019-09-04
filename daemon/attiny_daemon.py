#!/usr/bin/python3 -u

import logging
import os
import sys
import time
import smbus
import struct
from typing import Tuple, Any
from configparser import ConfigParser
from argparse import ArgumentParser, Namespace
from collections.abc import Mapping
from pathlib import Path
from attiny_i2c import ATTiny

### Global configuration of the daemon. You should know what you do if you change
### these values.

# Version information
major = 2
minor = 1
patch = 1

# config file is in the same directory as the script:
_configfile_default = str(Path(__file__).parent.absolute()) + "/attiny_daemon.cfg"
_shutdown_cmd = "sudo systemctl poweroff"  # sudo allows us to start as user 'pi'
_reboot_cmd = "sudo systemctl reboot"      # sudo allows us to start as user 'pi'
_time_const = 0.5  # used as a pause between i2c communications, the ATTiny is slow
_num_retries = 10  # the number of retries when reading from or writing to the ATTiny

# These are the different values reported back by the ATTiny depending on its config
button_level = 2**3
SL_INITIATED = 2  # the value we use to signal that we are shutting down
shutdown_levels = {
    # 0: Normal mode
    0: "Everything is normal.",
    # 2 is reserved for us signalling the ATTiny that we are shutting down
    # 4-15: Maybe shutdown or restart, depending on configuration
    2**2: "No external voltage detected. We are on battery power.",
    button_level: "Button has been pressed. Reacting according to configuration.",
    # >16: Definitely shut down
    2**7: "Battery is at warn level. Shutting down.",
}
# Here we store the button functions that are called depending on the configuration
button_functions = {
    "nothing": lambda: logging.info("Button pressed. Configured to do nothing."),
    "shutdown": lambda: os.system(_shutdown_cmd),
    "reboot": lambda: os.system(_reboot_cmd)
}

# this is the minimum reboot time we assume the RPi needs, used for a warning message
minimum_boot_time = 30

### Code starts here.
### Here be dragons...

bus = smbus.SMBus(1)


def main(*args):
    # Startup of the daemon

    args = parse_cmdline(args)
    setup_logger(args.nodaemon)

    config = Config(args.cfgfile)
    config.read_config()

    logging.info("ATTiny Daemon version " + str(major) + "." + str(minor) + "." + str(patch))

    attiny = ATTiny(bus, config[Config.I2C_ADDRESS], _time_const, _num_retries)

    if attiny.get_last_access() < 0:
        logging.error("Cannot access ATTiny")
        log_geekworm_voltage()
        exit(1)

    (a_major, a_minor, a_patch) = attiny.get_version()
    logging.info("ATTiny firmware version " + str(a_major) + "." + str(a_minor) + "." + str(a_patch))

    if major != a_major:
        logging.error("Daemon and Firmware major version mismatch. This might lead to serious problems. Check both versions.")

    config.merge_and_sync_values(attiny)

    # loop until stopped or error
    set_unprimed = False
    try:
        while True:
            should_shutdown = attiny.should_shutdown()
            if should_shutdown == 0xFF:
                # We have a big problem
                logging.error("Lost connection to ATTiny.")
                log_geekworm_voltage()
                set_unprimed = True        # we still try to reset primed
                exit(1)  # executes finally clause and lets the system restart the daemon

            if should_shutdown > SL_INITIATED:
                # we will not exit the process but wait for the systemd to shut us down
                # using SIGTERM. This does not execute the finally clause and leaves
                # everything as it is currently configured
                global shutdown_levels
                fallback = "Unknown shutdown_level " + str(should_shutdown) + ". Shutting down."
                logging.warning(shutdown_levels.get(should_shutdown, fallback))

                if should_shutdown > 16:
                    attiny.set_should_shutdown(SL_INITIATED) # we are shutting down
                    logging.info("shutting down now...")
                    os.system(_shutdown_cmd)
                elif (should_shutdown | button_level) != 0:
                    # we are executing the button command and setting the level to normal
                    attiny.set_should_shutdown(0)
                    button_functions[config[Config.BUTTON_FUNCTION]]()

            logging.debug("Sleeping for " + str(config[Config.SLEEPTIME]) + " seconds.")
            time.sleep(config[Config.SLEEPTIME])

    except KeyboardInterrupt:
        logging.info("Terminating daemon: cleaning up and exiting")
        # Ctrl-C means we do not run as daemon
        set_unprimed = True
    except Exception as e:
        logging.error("An exception occurred: '" + str(e) + "' Exiting...")
    finally:
        # will not be executed on SIGTERM, leaving primed set to the config value
        primed = config[Config.PRIMED]
        if args.nodaemon or set_unprimed:
            primed = False
        if primed == False:
            logging.info("Trying to reset primed flag")
        attiny.set_primed(primed)


def parse_cmdline(args: Tuple[Any]) -> Namespace:
    arg_parser = ArgumentParser(description='ATTiny Daemon')
    arg_parser.add_argument('--cfgfile', metavar='file', required=False,
                            help='full path and name of the configfile')
    arg_parser.add_argument('--nodaemon', required=False, action='store_true',
                            help='use normal output formatting')
    return arg_parser.parse_args(args)


def setup_logger(nodaemon: bool) -> None:
    root_log = logging.getLogger()
    root_log.setLevel("INFO")
    if not nodaemon:
        root_log.addHandler(SystemdHandler())


class SystemdHandler(logging.Handler):
    # http://0pointer.de/public/systemd-man/sd-daemon.html
    PREFIX = {
        # EMERG <0>
        # ALERT <1>
        logging.CRITICAL: "<2>",
        logging.ERROR: "<3>",
        logging.WARNING: "<4>",
        # NOTICE <5>
        logging.INFO: "<6>",
        logging.DEBUG: "<7>",
        logging.NOTSET: "<7>"
    }

    def __init__(self, stream=sys.stdout):
        self.stream = stream
        logging.Handler.__init__(self)

    def emit(self, record):
        try:
            msg = self.PREFIX[record.levelno] + self.format(record)
            msg = msg.replace("\n", "\\n")
            self.stream.write(msg + "\n")
            self.stream.flush()
        except Exception:
            self.handleError(record)


class Config(Mapping):
    DAEMON_SECTION = "attinydaemon"
    I2C_ADDRESS = 'i2c address'
    TIMEOUT = 'timeout'
    SLEEPTIME = 'sleeptime'
    PRIMED = 'primed'
    BAT_V_COEFFICIENT = 'battery voltage coefficient'
    BAT_V_CONSTANT = 'battery voltage constant'
    EXT_V_COEFFICIENT = 'external voltage coefficient'
    EXT_V_CONSTANT = 'external voltage constant'
    T_COEFFICIENT = 'temperature coefficient'
    T_CONSTANT = 'temperature constant'
    FORCE_SHUTDOWN = 'force shutdown'
    WARN_VOLTAGE = 'warn voltage'
    SHUTDOWN_VOLTAGE = 'shutdown voltage'
    RESTART_VOLTAGE = 'restart voltage'
    LOG_LEVEL = 'loglevel'
    BUTTON_FUNCTION = 'button function'

    MAX_INT = sys.maxsize
    DEFAULT_CONFIG = {
        DAEMON_SECTION: {
            I2C_ADDRESS: '0x37',
            TIMEOUT: str(MAX_INT),
            SLEEPTIME: str(MAX_INT),
            PRIMED: 'False',
            BAT_V_COEFFICIENT: str(MAX_INT),
            BAT_V_CONSTANT: str(MAX_INT),
            EXT_V_COEFFICIENT: str(MAX_INT),
            EXT_V_CONSTANT: str(MAX_INT),
            T_COEFFICIENT: str(MAX_INT),
            T_CONSTANT: str(MAX_INT),
            FORCE_SHUTDOWN: 'True',
            WARN_VOLTAGE: str(MAX_INT),
            SHUTDOWN_VOLTAGE: str(MAX_INT),
            RESTART_VOLTAGE: str(MAX_INT),
            BUTTON_FUNCTION: "nothing",
            LOG_LEVEL: 'DEBUG'
        }
    }

    def __init__(self, cfgfile):
        global _configfile_default  # simpler to change than a class variable
        if cfgfile:
            self.configfile_name = cfgfile
        else:
            self.configfile_name = _configfile_default
        self.config = {}
        self.parser = ConfigParser(allow_no_value=True)
        self._storage = dict()

    def __getitem__(self, key):
        return self._storage[key]

    def __iter__(self):
        return iter(self._storage)

    def __len__(self):
        return len(self._storage)

    def read_config(self):
        self.parser.read_dict(self.DEFAULT_CONFIG)

        if not os.path.isfile(self.configfile_name):
            logging.info("No Config File. Trying to create one.")
            # self.write_config()

        else:
            try:
                self.parser.read(self.configfile_name)
            except Exception:
                logging.warning("cannot read config file. Using default values")

        try:
            self._storage[self.I2C_ADDRESS] = int(self.parser.get(self.DAEMON_SECTION, self.I2C_ADDRESS), 0)
            self._storage[self.TIMEOUT] = self.parser.getint(self.DAEMON_SECTION, self.TIMEOUT)
            self._storage[self.SLEEPTIME] = self.parser.getint(self.DAEMON_SECTION, self.SLEEPTIME)
            self._storage[self.PRIMED] = self.parser.getboolean(self.DAEMON_SECTION, self.PRIMED)
            self._storage[self.BAT_V_COEFFICIENT] = self.parser.getint(self.DAEMON_SECTION, self.BAT_V_COEFFICIENT)
            self._storage[self.BAT_V_CONSTANT] = self.parser.getint(self.DAEMON_SECTION, self.BAT_V_CONSTANT)
            self._storage[self.EXT_V_COEFFICIENT] = self.parser.getint(self.DAEMON_SECTION, self.EXT_V_COEFFICIENT)
            self._storage[self.EXT_V_CONSTANT] = self.parser.getint(self.DAEMON_SECTION, self.EXT_V_CONSTANT)
            self._storage[self.T_COEFFICIENT] = self.parser.getint(self.DAEMON_SECTION, self.T_COEFFICIENT)
            self._storage[self.T_CONSTANT] = self.parser.getint(self.DAEMON_SECTION, self.T_CONSTANT)
            self._storage[self.FORCE_SHUTDOWN] = self.parser.getboolean(self.DAEMON_SECTION, self.FORCE_SHUTDOWN)
            self._storage[self.WARN_VOLTAGE] = self.parser.getint(self.DAEMON_SECTION, self.WARN_VOLTAGE)
            self._storage[self.SHUTDOWN_VOLTAGE] = self.parser.getint(self.DAEMON_SECTION, self.SHUTDOWN_VOLTAGE)
            self._storage[self.RESTART_VOLTAGE] = self.parser.getint(self.DAEMON_SECTION, self.RESTART_VOLTAGE)
            self._storage[self.BUTTON_FUNCTION] = self.parser.get(self.DAEMON_SECTION, self.BUTTON_FUNCTION)
            logging.getLogger().setLevel(self.parser.get(self.DAEMON_SECTION, self.LOG_LEVEL))
            logging.debug("config variables are set")
        except Exception as e:
            logging.error("Cannot convert option: " + str(e))
            exit(1)

    def write_config(self):
        try:
            cfgfile = open(self.configfile_name, 'w')
            self.parser.write(cfgfile)
            cfgfile.close()
        except Exception:
            logging.warning("cannot write config file.")

    @staticmethod
    def calc_sleeptime(val):
        global minimum_boot_time
        # we should have at least 30 seconds to boot
        # before the timeout occurs
        sleeptime = val - minimum_boot_time
        if sleeptime < 10:
            sleeptime = int(val / 2)
        if sleeptime < minimum_boot_time:
            logging.warning("Sleeptime is low. Ensure that the Raspberry can boot in " + str(sleeptime) + " seconds or change the config file.")
        return sleeptime

    # not the perfect place for the method, but good enough
    def merge_and_sync_values(self, attiny):
        logging.debug("Merge Values and save if necessary")
        changed_config = False

        attiny_primed = attiny.get_primed()
        attiny_timeout = attiny.get_timeout()
        attiny_force_shutdown = attiny.get_force_shutdown()

        if self._storage[self.TIMEOUT] == self.MAX_INT:
            # timeout was not set in the config file
            # we will get timeout, primed and force_shutdown
            # from the ATTiny
            logging.debug("Getting Timeout from ATTiny")
            self._storage[self.PRIMED] = attiny_primed
            self._storage[self.TIMEOUT] = attiny_timeout
            self._storage[self.FORCE_SHUTDOWN] = attiny_force_shutdown

            self.parser.set(self.DAEMON_SECTION, self.TIMEOUT,
                            str(self._storage[self.TIMEOUT]))
            self.parser.set(self.DAEMON_SECTION, self.PRIMED,
                            str(self._storage[self.PRIMED]))
            self.parser.set(self.DAEMON_SECTION, self.FORCE_SHUTDOWN,
                            str(self._storage[self.FORCE_SHUTDOWN]))
            changed_config = True
        else:
            if attiny_timeout != self._storage[self.TIMEOUT]:
                logging.debug("Writing Timeout to ATTiny")
                attiny.set_timeout(self._storage[self.TIMEOUT])
            if attiny_primed != self._storage[self.PRIMED]:
                logging.debug("Writing Primed to ATTiny")
                attiny.set_primed(self._storage[self.PRIMED])
            if attiny_force_shutdown != self._storage[self.FORCE_SHUTDOWN]:
                logging.debug("Writing Force_Shutdown to ATTiny")
                attiny.set_force_shutdown(self._storage[self.FORCE_SHUTDOWN])

        # check for max_int and only set if sleeptime is set to that value
        if self._storage[self.SLEEPTIME] == self.MAX_INT:
            logging.debug("Sleeptime not set, calculating from timeout value")
            self._storage[self.SLEEPTIME] = self.calc_sleeptime(self._storage[self.TIMEOUT])
            self.parser.set(self.DAEMON_SECTION, self.SLEEPTIME,
                            str(self._storage[self.SLEEPTIME]))
            logging.debug(self._storage[self.SLEEPTIME])
            changed_config = True

        if self._sync_Voltage(self.WARN_VOLTAGE, attiny, attiny.REG_WARN_VOLTAGE):
            changed_config = True

        if self._sync_Voltage(self.SHUTDOWN_VOLTAGE, attiny, attiny.REG_SHUTDOWN_VOLTAGE):
            changed_config = True

        if self._sync_Voltage(self.RESTART_VOLTAGE, attiny, attiny.REG_RESTART_VOLTAGE):
            changed_config = True

        if self._sync_Voltage(self.BAT_V_COEFFICIENT, attiny, attiny.REG_BAT_V_COEFFICIENT):
            changed_config = True

        if self._sync_Voltage(self.BAT_V_CONSTANT, attiny, attiny.REG_BAT_V_CONSTANT):
            changed_config = True

        if self._sync_Voltage(self.EXT_V_COEFFICIENT, attiny, attiny.REG_EXT_V_COEFFICIENT):
            changed_config = True

        if self._sync_Voltage(self.EXT_V_CONSTANT, attiny, attiny.REG_EXT_V_CONSTANT):
            changed_config = True

        if self._sync_Voltage(self.T_COEFFICIENT, attiny, attiny.REG_T_COEFFICIENT):
            changed_config = True

        if self._sync_Voltage(self.T_CONSTANT, attiny, attiny.REG_T_CONSTANT):
            changed_config = True

        if changed_config:
            logging.debug("Writing new config file")
            self.write_config()

    def _sync_Voltage(self, voltage_type, attiny, attiny_reg):
        attiny_voltage = attiny.get_16bit_value(attiny_reg)
        if self._storage[voltage_type] == self.MAX_INT:
            logging.debug("Getting Register " + hex(attiny_reg) + " from ATTiny")
            self._storage[voltage_type] = attiny_voltage
            self.parser.set(self.DAEMON_SECTION, voltage_type,
                            str(self._storage[voltage_type]))
            changed_config = True
        else:
            changed_config = False
            if attiny_voltage != self._storage[voltage_type]:
                logging.debug("Writing Register " + hex(attiny_reg) + " to ATTiny")
                attiny.set_16bit_value(attiny_reg, self._storage[voltage_type])
        return changed_config


def read_geekworm():
    try:
        address = 0x36
        read = bus.read_word_data(address, 2)
        swapped = struct.unpack("<H", struct.pack(">H", read))[0]
        voltage = swapped * 78.125 / 1000000
        return voltage
    except Exception:
        return 0


def log_geekworm_voltage():
    gw_voltage = read_geekworm()
    if gw_voltage == 0:
        logging.error("Cannot access Geekworm")
    else:
        logging.info("Geekworm voltage is " + str(gw_voltage))


if __name__ == '__main__':
    main(*sys.argv[1:])
