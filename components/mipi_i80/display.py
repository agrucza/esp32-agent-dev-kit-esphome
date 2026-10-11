"""mipi_i80: MIPI DBI Type B (8080 / i80) parallel display driver built on ESP-IDF esp_lcd.

Written for the ST7796U panel of the Wireless-Tag ESP32 Agent Dev Kit (AD35-S3).
"""

from esphome import pins
import esphome.codegen as cg
from esphome.components import display
from esphome.components.display import validate_rotation
from esphome.components.esp32 import (
    VARIANT_ESP32S3,
    include_builtin_idf_component,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_ORDER,
    CONF_CS_PIN,
    CONF_DATA_PINS,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INIT_SEQUENCE,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_OFFSET_HEIGHT,
    CONF_OFFSET_WIDTH,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_WIDTH,
)
from esphome.types import ConfigType

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@agrucza"]

CONF_WR_PIN = "wr_pin"
CONF_PCLK_FREQUENCY = "pclk_frequency"
CONF_ALT_INIT_SEQUENCE = "alt_init_sequence"
CONF_PANEL_SELECT_PINS = "panel_select_pins"

mipi_i80_ns = cg.esphome_ns.namespace("mipi_i80")
MipiI80 = mipi_i80_ns.class_(
    "MipiI80", cg.PollingComponent, display.Display, display.DisplayBuffer
)
ColorOrder = display.display_ns.enum("ColorMode")
COLOR_ORDERS = {
    "RGB": ColorOrder.COLOR_ORDER_RGB,
    "BGR": ColorOrder.COLOR_ORDER_BGR,
}

DELAY_FLAG = 0xFF


def cmd(c, *args):
    return [c, len(args), *args]


def delay_ms(ms):
    return [ms, DELAY_FLAG]


# Vendor init sequences for the AD35-S3 panel (from the Wireless-Tag example firmware,
# qmsd_lcd_reg_config / qmsd_lcd_reg_config_v2). MADCTL (0x36) is left out; the driver
# writes it from the transform settings. The firmware picks v2 unless the panel strap
# (data lines D0/D3 read with pull-ups before bus init) equals 0x11.
_COMMON_TAIL = [
    *cmd(0xE8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33),
]

ST7796_AD35_V1 = [
    *cmd(0x11),
    *delay_ms(120),
    *cmd(0x3A, 0x55),
    *cmd(0xF0, 0xC3),
    *cmd(0xF0, 0x96),
    *cmd(0xB4, 0x01),
    *cmd(0xB5, 0x1E),
    *cmd(0xB7, 0xC6),
    *cmd(0xB9, 0x02, 0xE0),
    *cmd(0xC0, 0x80, 0x16),
    *cmd(0xC1, 0x19),
    *cmd(0xC2, 0xA7),
    *cmd(0xC5, 0x16),
    *_COMMON_TAIL,
    *cmd(0xE0, 0xF0, 0x07, 0x0D, 0x04, 0x05, 0x14, 0x36, 0x54, 0x4C, 0x38, 0x13, 0x14, 0x2E, 0x34),
    *cmd(0xE1, 0xF0, 0x10, 0x14, 0x0E, 0x0C, 0x08, 0x35, 0x44, 0x4C, 0x26, 0x10, 0x12, 0x2C, 0x32),
    *cmd(0xF0, 0x3C),
    *cmd(0xF0, 0x69),
    *delay_ms(120),
    *cmd(0x29),
    *cmd(0x35),
]

ST7796_AD35_V2 = [
    *cmd(0x11),
    *delay_ms(120),
    *cmd(0x3A, 0x55),
    *cmd(0xF0, 0xC3),
    *cmd(0xF0, 0x96),
    *cmd(0xB1, 0x90, 0x18),
    *cmd(0xB4, 0x01),
    *cmd(0xB7, 0xC6),
    *cmd(0xB9, 0x02, 0xE0),
    *cmd(0xC0, 0x00, 0x00),
    *cmd(0xC1, 0x16),
    *cmd(0xC6, 0x80),
    *cmd(0xC2, 0xA7),
    *cmd(0xC5, 0x00),
    *_COMMON_TAIL,
    *cmd(0xE0, 0xF0, 0x09, 0x0F, 0x0D, 0x0D, 0x1C, 0x3D, 0x44, 0x55, 0x39, 0x18, 0x18, 0x36, 0x39),
    *cmd(0xE1, 0xF0, 0x09, 0x0F, 0x09, 0x08, 0x01, 0x31, 0x33, 0x46, 0x09, 0x13, 0x13, 0x2A, 0x31),
    *cmd(0xF0, 0x3C),
    *cmd(0xF0, 0x69),
    *delay_ms(120),
    *cmd(0x29),
]

# model name -> (width, height, primary sequence, alternate sequence, invert_colors default)
MODELS = {
    "ST7796_AD35": (320, 480, ST7796_AD35_V1, ST7796_AD35_V2, True),
    "CUSTOM": (None, None, None, None, False),
}


def gpio_number(value):
    """Accept 9 or GPIO9 without registering the pin as used."""
    if isinstance(value, str) and value.upper().startswith("GPIO"):
        value = value[4:]
    return cv.int_range(min=0, max=48)(value)


def map_sequence(value):
    if not isinstance(value, list) or len(value) == 0:
        raise cv.Invalid("Each init sequence entry must be a non-empty list [cmd, data...]")
    return cmd(*value)


def _validate(config: ConfigType) -> ConfigType:
    if config[CONF_MODEL] == "CUSTOM":
        if CONF_INIT_SEQUENCE not in config or CONF_DIMENSIONS not in config:
            raise cv.Invalid("CUSTOM model requires init_sequence and dimensions")
    if len(config[CONF_DATA_PINS]) != 8:
        raise cv.Invalid("Exactly 8 data_pins are required (8-bit bus)")
    return config


def _register_metadata(config: ConfigType) -> ConfigType:
    """Tell display consumers (LVGL) about size, byte order and rotation support."""
    width, height, _, _, _ = MODELS[config[CONF_MODEL]]
    if CONF_DIMENSIONS in config:
        dims = config[CONF_DIMENSIONS]
        width, height = (
            (dims[CONF_WIDTH], dims[CONF_HEIGHT]) if isinstance(dims, dict) else dims
        )
    if config.get(CONF_TRANSFORM, {}).get(CONF_SWAP_XY):
        width, height = height, width
    display.add_metadata(
        config[CONF_ID],
        width,
        height,
        has_hardware_rotation=False,
        byte_order="big_endian",
        has_writer=CONF_LAMBDA in config or CONF_PAGES in config,
        rotation=config.get(CONF_ROTATION, 0),
        draw_rounding=1,
    )
    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MipiI80),
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
            cv.Required(CONF_DATA_PINS): cv.ensure_list(pins.internal_gpio_output_pin_number),
            cv.Required(CONF_WR_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_DC_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_CS_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_PCLK_FREQUENCY, default="20MHz"): cv.All(
                cv.frequency, cv.Range(min=1e6, max=80e6)
            ),
            # Plain GPIO numbers on purpose: these are two of the data lines, sampled as
            # inputs before the bus is set up, so the pin registry must not see them twice.
            cv.Optional(CONF_PANEL_SELECT_PINS): cv.All(
                cv.ensure_list(gpio_number), cv.Length(min=2, max=2)
            ),
            cv.Optional(CONF_DIMENSIONS): cv.Any(
                cv.dimensions,
                cv.Schema(
                    {
                        cv.Required(CONF_WIDTH): cv.int_,
                        cv.Required(CONF_HEIGHT): cv.int_,
                        cv.Optional(CONF_OFFSET_HEIGHT, default=0): cv.int_,
                        cv.Optional(CONF_OFFSET_WIDTH, default=0): cv.int_,
                    }
                ),
            ),
            cv.Optional(CONF_INVERT_COLORS): cv.boolean,
            cv.Optional(CONF_COLOR_ORDER, default="BGR"): cv.one_of(*COLOR_ORDERS, upper=True),
            cv.Exclusive(CONF_ROTATION, CONF_ROTATION): validate_rotation,
            cv.Exclusive(CONF_TRANSFORM, CONF_ROTATION): cv.Schema(
                {
                    cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
                }
            ),
            cv.Optional(CONF_INIT_SEQUENCE): cv.ensure_list(map_sequence),
            cv.Optional(CONF_ALT_INIT_SEQUENCE): cv.ensure_list(map_sequence),
        }
    ).extend(cv.polling_component_schema("1s")),
    cv.only_on_esp32,
    only_on_variant(supported=[VARIANT_ESP32S3]),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    _validate,
    _register_metadata,
)


def _flatten(seqs):
    out = []
    for s in seqs:
        out.extend(s)
    return out


async def to_code(config: ConfigType) -> None:
    include_builtin_idf_component("esp_lcd")
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    width, height, seq, alt_seq, invert_default = MODELS[config[CONF_MODEL]]
    if CONF_DIMENSIONS in config:
        dims = config[CONF_DIMENSIONS]
        if isinstance(dims, dict):
            width, height = dims[CONF_WIDTH], dims[CONF_HEIGHT]
            cg.add(var.set_offsets(dims[CONF_OFFSET_WIDTH], dims[CONF_OFFSET_HEIGHT]))
        else:
            width, height = dims
    cg.add(var.set_dimensions(width, height))

    if CONF_INIT_SEQUENCE in config:
        seq = _flatten(config[CONF_INIT_SEQUENCE])
    if CONF_ALT_INIT_SEQUENCE in config:
        alt_seq = _flatten(config[CONF_ALT_INIT_SEQUENCE])
    cg.add(var.set_init_sequence(seq))
    if alt_seq:
        cg.add(var.set_alt_init_sequence(alt_seq))

    cg.add(var.set_data_pins(config[CONF_DATA_PINS]))
    cg.add(var.set_wr_pin(config[CONF_WR_PIN]))
    cg.add(var.set_dc_pin(config[CONF_DC_PIN]))
    cg.add(var.set_cs_pin(config.get(CONF_CS_PIN, -1)))
    cg.add(var.set_pclk_frequency(int(config[CONF_PCLK_FREQUENCY])))
    if sel := config.get(CONF_PANEL_SELECT_PINS):
        cg.add(var.set_panel_select_pins(sel[0], sel[1]))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    cg.add(var.set_invert_colors(config.get(CONF_INVERT_COLORS, invert_default)))
    cg.add(var.set_color_order(COLOR_ORDERS[config[CONF_COLOR_ORDER]]))
    if CONF_TRANSFORM in config:
        t = config[CONF_TRANSFORM]
        cg.add(var.set_swap_xy(t[CONF_SWAP_XY]))
        cg.add(var.set_mirror_x(t[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(t[CONF_MIRROR_Y]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
