#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "../aw9523.h"

namespace esphome::aw9523 {

class AW9523Output : public output::FloatOutput, public Component {
 public:
  void set_parent(AW9523Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override { this->parent_->set_led_mode(this->pin_); }
  void dump_config() override;

 protected:
  void write_state(float state) override {
    this->parent_->set_led_level(this->pin_, static_cast<uint8_t>(state * 255.0f + 0.5f));
  }
  AW9523Component *parent_{nullptr};
  uint8_t pin_{0};
};

}  // namespace esphome::aw9523
