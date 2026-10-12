"""MK-PB2016PS-A01E 16-bit I2C proximity sensor (Shenzhen Maike Optoelectronics), as fitted
to the Wireless-Tag ESP32 Agent Dev Kit. Reports the raw proximity count."""

import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import ICON_MOTION_SENSOR, STATE_CLASS_MEASUREMENT

DEPENDENCIES = ["i2c"]

mkpb2016ps_ns = cg.esphome_ns.namespace("mkpb2016ps")
MKPB2016PS = mkpb2016ps_ns.class_(
    "MKPB2016PS", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

CONF_LED_DRIVE = "led_drive"
CONF_PULSE_COUNT = "pulse_count"
CONF_GAIN = "gain"

LED_DRIVE = {"125mA": 0, "100mA": 1, "75mA": 2, "50mA": 3}
GAIN = {1: 0, 2: 1, 4: 2}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MKPB2016PS,
        accuracy_decimals=0,
        icon=ICON_MOTION_SENSOR,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_LED_DRIVE, default="50mA"): cv.enum(LED_DRIVE),
            cv.Optional(CONF_PULSE_COUNT, default=8): cv.int_range(min=1, max=255),
            cv.Optional(CONF_GAIN, default=1): cv.enum(GAIN, int=True),
        }
    )
    .extend(cv.polling_component_schema("250ms"))
    .extend(i2c.i2c_device_schema(0x39))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_led_drive(config[CONF_LED_DRIVE]))
    cg.add(var.set_pulse_count(config[CONF_PULSE_COUNT]))
    cg.add(var.set_gain(config[CONF_GAIN]))
