#include "aw9523.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::aw9523 {

static const char *const TAG = "aw9523";

static constexpr uint8_t REG_INPUT0 = 0x00;
static constexpr uint8_t REG_OUTPUT0 = 0x02;
static constexpr uint8_t REG_CONFIG0 = 0x04;
static constexpr uint8_t REG_INTENABLE0 = 0x06;
static constexpr uint8_t REG_CHIPID = 0x10;
static constexpr uint8_t REG_GCR = 0x11;
static constexpr uint8_t REG_LEDMODE0 = 0x12;
static constexpr uint8_t REG_SOFTRESET = 0x7F;
static constexpr uint8_t CHIP_ID = 0x23;
static constexpr uint8_t GCR_P0_PUSH_PULL = 0x10;

// Dimming register for each pin (datasheet table "256 step dimming control")
static uint8_t dim_reg(uint8_t pin) {
  if (pin < 8)
    return 0x24 + pin;  // P0_0..P0_7
  if (pin < 12)
    return 0x20 + (pin - 8);  // P1_0..P1_3
  return 0x2C + (pin - 12);   // P1_4..P1_7
}

void AW9523Component::setup() {
  uint8_t id = 0;
  if (!this->read_byte(REG_CHIPID, &id) || id != CHIP_ID) {
    ESP_LOGE(TAG, "Chip ID read failed or wrong (0x%02X)", id);
    this->mark_failed();
    return;
  }
  this->write_byte(REG_SOFTRESET, 0x00);
  delay(1);
  // all pins input, GPIO mode, interrupts off, port 0 push-pull, current limit
  this->config_[0] = this->config_[1] = 0xFF;
  this->ledmode_[0] = this->ledmode_[1] = 0xFF;
  this->output_[0] = this->output_[1] = 0x00;
  this->write_byte(REG_CONFIG0, 0xFF);
  this->write_byte(REG_CONFIG0 + 1, 0xFF);
  this->write_byte(REG_LEDMODE0, 0xFF);
  this->write_byte(REG_LEDMODE0 + 1, 0xFF);
  this->write_byte(REG_INTENABLE0, 0xFF);
  this->write_byte(REG_INTENABLE0 + 1, 0xFF);
  this->write_byte(REG_GCR, GCR_P0_PUSH_PULL | this->current_limit_);
}

void AW9523Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AW9523:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  LED current limit: %.2f mA", 37.0f * (4 - this->current_limit_) / 4);
  if (this->is_failed())
    ESP_LOGE(TAG, "  Communication failed");
}

bool AW9523Component::update_bit_(uint8_t reg, uint8_t bit, bool value) {
  uint8_t *shadow;
  switch (reg & 0xFE) {
    case REG_OUTPUT0:
      shadow = this->output_;
      break;
    case REG_CONFIG0:
      shadow = this->config_;
      break;
    case REG_LEDMODE0:
      shadow = this->ledmode_;
      break;
    default:
      return false;
  }
  uint8_t &byte = shadow[reg & 1];
  uint8_t nv = value ? (byte | (1 << bit)) : (byte & ~(1 << bit));
  if (nv == byte)
    return true;
  byte = nv;
  return this->write_byte(reg, byte);
}

void AW9523Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (this->is_failed())
    return;
  uint8_t port = pin >> 3, bit = pin & 7;
  // GPIO mode (1) and direction (1 = input)
  this->update_bit_(REG_LEDMODE0 + port, bit, true);
  this->update_bit_(REG_CONFIG0 + port, bit, (flags & gpio::FLAG_OUTPUT) == 0);
}

void AW9523Component::digital_write(uint8_t pin, bool value) {
  if (this->is_failed())
    return;
  this->update_bit_(REG_OUTPUT0 + (pin >> 3), pin & 7, value);
}

bool AW9523Component::digital_read(uint8_t pin) {
  if (this->is_failed())
    return false;
  uint8_t v = 0;
  if (!this->read_byte(REG_INPUT0 + (pin >> 3), &v))
    return false;
  return (v >> (pin & 7)) & 1;
}

void AW9523Component::set_led_mode(uint8_t pin) {
  if (this->is_failed())
    return;
  uint8_t port = pin >> 3, bit = pin & 7;
  this->update_bit_(REG_CONFIG0 + port, bit, false);   // output
  this->update_bit_(REG_LEDMODE0 + port, bit, false);  // LED mode (0)
  this->write_byte(dim_reg(pin), 0);
}

void AW9523Component::set_led_level(uint8_t pin, uint8_t level) {
  if (this->is_failed())
    return;
  this->write_byte(dim_reg(pin), level);
}

size_t AW9523GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via AW9523", this->pin_);
}

}  // namespace esphome::aw9523
