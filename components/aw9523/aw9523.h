// AW9523B 16-channel I2C GPIO expander / LED driver for ESPHome.
// Pin numbering: 0-7 = P0_0..P0_7, 8-15 = P1_0..P1_7.
#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::aw9523 {

class AW9523Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  void set_led_current_limit(uint8_t divider) { this->current_limit_ = divider & 0x03; }

  // GPIO mode
  void pin_mode(uint8_t pin, gpio::Flags flags);
  void digital_write(uint8_t pin, bool value);
  bool digital_read(uint8_t pin);

  // LED (constant current, 256 step) mode
  void set_led_mode(uint8_t pin);
  void set_led_level(uint8_t pin, uint8_t level);

 protected:
  bool update_bit_(uint8_t reg, uint8_t bit, bool value);
  uint8_t current_limit_{0};
  uint8_t output_[2]{0, 0};
  uint8_t config_[2]{0xFF, 0xFF};
  uint8_t ledmode_[2]{0xFF, 0xFF};
};

class AW9523GPIOPin : public GPIOPin {
 public:
  void setup() override { this->pin_mode(this->flags_); }
  void pin_mode(gpio::Flags flags) override { this->parent_->pin_mode(this->pin_, flags); }
  bool digital_read() override { return this->parent_->digital_read(this->pin_) != this->inverted_; }
  void digital_write(bool value) override { this->parent_->digital_write(this->pin_, value != this->inverted_); }
  size_t dump_summary(char *buffer, size_t len) const override;
  gpio::Flags get_flags() const override { return this->flags_; }

  void set_parent(AW9523Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

 protected:
  AW9523Component *parent_{nullptr};
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{gpio::FLAG_NONE};
};

}  // namespace esphome::aw9523
