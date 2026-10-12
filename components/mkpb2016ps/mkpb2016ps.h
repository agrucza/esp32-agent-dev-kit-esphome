// MK-PB2016PS-A01E proximity sensor. The command byte carries the register address in bits 4:0
// and the transaction type in bits 7:5 (000 = single register, 001 = auto-increment).
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::mkpb2016ps {

class MKPB2016PS : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_led_drive(uint8_t v) { this->led_drive_ = v; }
  void set_pulse_count(uint8_t v) { this->pulse_count_ = v; }
  void set_gain(uint8_t v) { this->gain_ = v; }

 protected:
  bool write_reg_(uint8_t reg, uint8_t value);
  bool read_reg_(uint8_t reg, uint8_t *value);

  uint8_t led_drive_{3};
  uint8_t pulse_count_{8};
  uint8_t gain_{0};
  uint8_t debug_reads_{20};
};

}  // namespace esphome::mkpb2016ps
