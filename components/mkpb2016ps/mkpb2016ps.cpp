#include "mkpb2016ps.h"
#include "esphome/core/log.h"

namespace esphome::mkpb2016ps {

static const char *const TAG = "mkpb2016ps";

// Command byte: bits 7:5 = transaction type (000 repeated byte, 001 auto-increment), bits 4:0 = register.
static constexpr uint8_t CMD_REPEATED = 0x00;
static constexpr uint8_t CMD_AUTO_INC = 0x20;

static constexpr uint8_t REG_ENABLE = 0x00;
static constexpr uint8_t REG_PTIME = 0x02;
static constexpr uint8_t REG_WTIME = 0x03;
static constexpr uint8_t REG_CONFIG = 0x0D;
static constexpr uint8_t REG_PPULSE = 0x0E;
static constexpr uint8_t REG_CONTROL = 0x0F;
static constexpr uint8_t REG_PWIDTH = 0x10;
static constexpr uint8_t REG_ID = 0x12;
static constexpr uint8_t REG_STATUS = 0x13;
static constexpr uint8_t REG_PDATAL = 0x18;

static constexpr uint8_t ENABLE_PON = 0x01;
static constexpr uint8_t ENABLE_PEN = 0x04;
static constexpr uint8_t ENABLE_WEN = 0x08;
static constexpr uint8_t STATUS_PVALID = 0x02;
static constexpr uint8_t CHIP_ID = 0x03;

bool MKPB2016PS::write_reg_(uint8_t reg, uint8_t value) {
  return this->write_byte(CMD_REPEATED | reg, value);
}

bool MKPB2016PS::read_reg_(uint8_t reg, uint8_t *value) {
  return this->read_byte(CMD_REPEATED | reg, value);
}

void MKPB2016PS::setup() {
  uint8_t id = 0;
  if (!this->read_reg_(REG_ID, &id) || id != CHIP_ID) {
    ESP_LOGE(TAG, "ID read failed or unexpected (0x%02X)", id);
    this->mark_failed();
    return;
  }
  // Restore the datasheet power-up defaults in the documented order (an OTA does not
  // power-cycle the sensor): oscillator on first, then timing and drive, then the engine.
  this->write_reg_(REG_ENABLE, ENABLE_PON);
  delay(3);
  this->write_reg_(REG_PTIME, 0xFF);
  this->write_reg_(REG_WTIME, 0xFF);
  this->write_reg_(REG_CONFIG, 0x00);
  this->write_reg_(REG_PPULSE, this->pulse_count_);
  this->write_reg_(REG_CONTROL, (uint8_t) ((this->led_drive_ << 6) | (this->gain_ << 2)));
  this->write_reg_(REG_PWIDTH, 0x00);
  this->write_reg_(REG_ENABLE, ENABLE_PON | ENABLE_PEN | ENABLE_WEN);
  uint8_t enable = 0;
  this->read_reg_(REG_ENABLE, &enable);
  ESP_LOGD(TAG, "ENABLE=0x%02X after setup", enable);
}

void MKPB2016PS::update() {
  if (this->is_failed())
    return;
  uint8_t lo = 0, hi = 0, status = 0;
  if (!this->read_reg_(REG_PDATAL, &lo) || !this->read_reg_(REG_PDATAL + 1, &hi)) {
    this->status_set_warning("data read failed");
    return;
  }
  this->status_clear_warning();
  if (this->debug_reads_ > 0) {
    this->read_reg_(REG_STATUS, &status);
    ESP_LOGD(TAG, "status=0x%02X pdata=%u", status, (unsigned) (lo | (hi << 8)));
    this->debug_reads_--;
  }
  this->publish_state(static_cast<float>(lo | (hi << 8)));
}

void MKPB2016PS::dump_config() {
  LOG_SENSOR("", "MK-PB2016PS proximity", this);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  LED drive: %u, pulses: %u, gain: %ux", this->led_drive_, this->pulse_count_, 1u << this->gain_);
  if (this->is_failed())
    ESP_LOGE(TAG, "  Communication failed");
}

}  // namespace esphome::mkpb2016ps
