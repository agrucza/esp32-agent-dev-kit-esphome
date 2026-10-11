"""AW9523 LED-mode float output (256-step constant current dimming)."""

import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN

from .. import CONF_AW9523, AW9523Component, aw9523_ns

DEPENDENCIES = ["aw9523"]

AW9523Output = aw9523_ns.class_("AW9523Output", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(AW9523Output),
        cv.Required(CONF_AW9523): cv.use_id(AW9523Component),
        cv.Required(CONF_PIN): cv.int_range(min=0, max=15),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    parent = await cg.get_variable(config[CONF_AW9523])
    cg.add(var.set_parent(parent))
    cg.add(var.set_pin(config[CONF_PIN]))
