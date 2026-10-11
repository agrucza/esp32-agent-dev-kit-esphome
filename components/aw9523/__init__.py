"""AW9523B 16-channel I2C GPIO expander / LED driver (Awinic).

Hub component plus a GPIO pin schema so the expander's pins can be used wherever
ESPHome accepts a pin (switch.gpio, binary_sensor.gpio, display reset pins, ...).
The companion output platform drives pins in 256-step LED (constant current) mode.
"""

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
)

CODEOWNERS = ["@agrucza"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_AW9523 = "aw9523"
CONF_LED_CURRENT_LIMIT = "led_current_limit"

aw9523_ns = cg.esphome_ns.namespace("aw9523")
AW9523Component = aw9523_ns.class_("AW9523Component", cg.Component, i2c.I2CDevice)
AW9523GPIOPin = aw9523_ns.class_("AW9523GPIOPin", cg.GPIOPin)

# GCR[1:0]: maximum LED current per channel
LED_CURRENT_LIMITS = {
    "37mA": 0,
    "27.75mA": 1,
    "18.5mA": 2,
    "9.25mA": 3,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(AW9523Component),
            cv.Optional(CONF_LED_CURRENT_LIMIT, default="37mA"): cv.enum(
                LED_CURRENT_LIMITS
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x58))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_led_current_limit(config[CONF_LED_CURRENT_LIMIT]))


def _validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


AW9523_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(AW9523GPIOPin),
        cv.Required(CONF_AW9523): cv.use_id(AW9523Component),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=15),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
            },
            _validate_mode,
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_AW9523, AW9523_PIN_SCHEMA)
async def aw9523_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_AW9523])
    cg.add(var.set_parent(parent))
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
