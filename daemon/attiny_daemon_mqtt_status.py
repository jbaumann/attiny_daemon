#!/usr/bin/python3 -u

import smbus
import paho.mqtt.publish as publish
import paho.mqtt.client as mqtt
import logging

from attiny_i2c import ATTiny

# This short script logs the current temperature and battery voltage to MQTT in JSON-format.
# Change the following settings to your needs and add the following line to the
# crontab of the user pi (without the leading hash-sign):
# * * * * * /opt/attiny_daemon/attiny_daemon_mqtt_status.py

# Settings specific to MQTT
_topic = "topic"
_hostname = "localhost"
_port = 1883
_client_id = ""
_user = None
_password = None
_additional_info = '"hostname" : "myhost"'

# Settings specific to ATTiny_Daemon
_time_const = 0.5   # used as a pause between i2c communications, the ATTiny is slow
_num_retries = 10   # the number of retries when reading from or writing to the ATTiny_Daemon
_i2c_address = 0x37 # the I2C address that is used for the ATTiny_Daemon

### Here begins the code

# set up logging
root_log = logging.getLogger()
root_log.setLevel("INFO")

# set up communication to the ATTiny_Daemon
bus = smbus.SMBus(1)
attiny = ATTiny(bus, _i2c_address, _time_const, _num_retries)

# access data, an error is signalled by a return value of 0xFFFFFFFF/4294967295
temperature = str(attiny.get_temperature())
voltage = str(attiny.get_bat_voltage())

#build output
json_string = '{"temperature" : ' + temperature  \
              + ', "battery_voltage" : ' + voltage + ', ' \
              + _additional_info + '}'

# build auth dict
_auth = None
if _user != None:
    _auth = {'username':_user, 'password':_password}

# send data to MQTT
publish.single(_topic, payload=json_string, qos=0, retain=False, hostname=_hostname, port=_port, client_id=_client_id, keepalive=60, will=None, auth=_auth, tls=None, protocol=mqtt.MQTTv311, transport="tcp")

