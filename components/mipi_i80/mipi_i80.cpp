// MIPI DBI Type B (8080 / i80) parallel display driver for ESPHome, built on ESP-IDF's esp_lcd i80 bus.
// See mipi_i80.h for details.
#include "mipi_i80.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>

namespace esphome::mipi_i80 {

static bool IRAM_ATTR color_trans_done_cb(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx) {
  return static_cast<MipiI80 *>(user_ctx)->on_color_trans_done_();
}

bool MipiI80::on_color_trans_done_() {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(this->done_, &woken);
  return woken == pdTRUE;
}

void MipiI80::setup() {
  this->width_ = this->swap_xy_ ? this->native_height_ : this->native_width_;
  this->height_ = this->swap_xy_ ? this->native_width_ : this->native_height_;

  // The vendor firmware reads two data lines (with pull-ups) before the bus is configured to
  // detect which panel revision is fitted, and picks the init sequence from that.
  if (this->sel_hi_pin_ >= 0 && this->sel_lo_pin_ >= 0) {
    for (int8_t p : {this->sel_hi_pin_, this->sel_lo_pin_}) {
      gpio_reset_pin(static_cast<gpio_num_t>(p));
      gpio_set_direction(static_cast<gpio_num_t>(p), GPIO_MODE_INPUT);
      gpio_set_pull_mode(static_cast<gpio_num_t>(p), GPIO_PULLUP_ONLY);
    }
    delay(1);
    this->panel_select_ = (gpio_get_level(static_cast<gpio_num_t>(this->sel_hi_pin_)) << 4) |
                          gpio_get_level(static_cast<gpio_num_t>(this->sel_lo_pin_));
    ESP_LOGD(TAG, "Panel select strap: 0x%02X", this->panel_select_);
  }

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }

  if (!this->init_bus_()) {
    this->mark_failed();
    return;
  }

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(20);
    this->reset_pin_->digital_write(true);
    delay(120);
  }

  const auto &seq = (this->panel_select_ != 0x11 && !this->alt_init_sequence_.empty()) ? this->alt_init_sequence_
                                                                                        : this->init_sequence_;
  this->run_init_sequence_(seq);
  this->set_madctl_();
  this->write_command_(this->invert_colors_ ? CMD_INVON : CMD_INVOFF);

  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

bool MipiI80::init_bus_() {
  if (this->data_pins_.size() != 8) {
    ESP_LOGE(TAG, "Exactly 8 data pins are required");
    return false;
  }
  this->done_ = xSemaphoreCreateBinary();

  this->bounce_size_ = static_cast<size_t>(this->native_width_) * BOUNCE_LINES * 2;
  this->bounce_ = static_cast<uint8_t *>(heap_caps_malloc(this->bounce_size_, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (this->bounce_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate %u byte DMA bounce buffer", static_cast<unsigned>(this->bounce_size_));
    return false;
  }

  esp_lcd_i80_bus_config_t bus_cfg{};
  bus_cfg.dc_gpio_num = this->dc_pin_;
  bus_cfg.wr_gpio_num = this->wr_pin_;
  bus_cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  for (size_t i = 0; i < 8; i++)
    bus_cfg.data_gpio_nums[i] = this->data_pins_[i];
  bus_cfg.bus_width = 8;
  bus_cfg.max_transfer_bytes = this->bounce_size_;
  bus_cfg.dma_burst_size = 64;

  esp_err_t err = esp_lcd_new_i80_bus(&bus_cfg, &this->bus_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_new_i80_bus failed: %s", esp_err_to_name(err));
    return false;
  }

  esp_lcd_panel_io_i80_config_t io_cfg{};
  io_cfg.cs_gpio_num = this->cs_pin_;
  io_cfg.pclk_hz = this->pclk_hz_;
  io_cfg.trans_queue_depth = 4;
  io_cfg.on_color_trans_done = color_trans_done_cb;
  io_cfg.user_ctx = this;
  io_cfg.lcd_cmd_bits = 8;
  io_cfg.lcd_param_bits = 8;
  io_cfg.dc_levels.dc_idle_level = 0;
  io_cfg.dc_levels.dc_cmd_level = 0;
  io_cfg.dc_levels.dc_dummy_level = 0;
  io_cfg.dc_levels.dc_data_level = 1;
  io_cfg.flags.swap_color_bytes = 0;  // we feed big-endian RGB565, which is what the panel wants
  io_cfg.flags.pclk_active_neg = 0;
  io_cfg.flags.pclk_idle_low = 0;

  err = esp_lcd_new_panel_io_i80(this->bus_, &io_cfg, &this->io_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_new_panel_io_i80 failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

void MipiI80::run_init_sequence_(const std::vector<uint8_t> &seq) {
  size_t i = 0;
  while (i + 1 < seq.size()) {
    uint8_t cmd = seq[i++];
    uint8_t n = seq[i++];
    if (n == DELAY_FLAG) {
      ESP_LOGV(TAG, "Delay %u ms", cmd);
      delay(cmd);
      continue;
    }
    if (i + n > seq.size()) {
      ESP_LOGE(TAG, "Malformed init sequence");
      return;
    }
    this->write_command_(cmd, seq.data() + i, n);
    i += n;
  }
}

void MipiI80::write_command_(uint8_t cmd, const uint8_t *data, size_t len) {
  this->wait_transfer_();
  esp_err_t err = esp_lcd_panel_io_tx_param(this->io_, cmd, data, len);
  if (err != ESP_OK)
    ESP_LOGE(TAG, "tx_param 0x%02X failed: %s", cmd, esp_err_to_name(err));
}

void MipiI80::set_madctl_() {
  uint8_t mad = this->color_order_ == display::COLOR_ORDER_BGR ? MADCTL_BGR : 0;
  if (this->swap_xy_)
    mad |= MADCTL_MV;
  if (this->mirror_x_)
    mad |= MADCTL_MX;
  if (this->mirror_y_)
    mad |= MADCTL_MY;
  this->write_command_(CMD_MADCTL, mad);
  ESP_LOGD(TAG, "MADCTL = 0x%02X", mad);
}

void MipiI80::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  x1 += this->offset_x_;
  x2 += this->offset_x_;
  y1 += this->offset_y_;
  y2 += this->offset_y_;
  uint8_t buf[4];
  buf[0] = x1 >> 8;
  buf[1] = x1;
  buf[2] = x2 >> 8;
  buf[3] = x2;
  this->write_command_(CMD_CASET, buf, 4);
  buf[0] = y1 >> 8;
  buf[1] = y1;
  buf[2] = y2 >> 8;
  buf[3] = y2;
  this->write_command_(CMD_PASET, buf, 4);
}

void MipiI80::wait_transfer_() {
  if (this->transfer_pending_) {
    xSemaphoreTake(this->done_, pdMS_TO_TICKS(1000));
    this->transfer_pending_ = false;
  }
}

// Copy rows through the internal DMA bounce buffer and push them with RAMWR / continue.
void MipiI80::send_rows_(const uint8_t *src, size_t w, size_t h, size_t stride) {
  const size_t row_bytes = w * 2;
  const size_t rows_per_chunk = std::max<size_t>(1, this->bounce_size_ / row_bytes);
  int cmd = CMD_RAMWR;
  size_t y = 0;
  while (y < h) {
    size_t rows = std::min(rows_per_chunk, h - y);
    this->wait_transfer_();
    for (size_t r = 0; r < rows; r++)
      memcpy(this->bounce_ + r * row_bytes, src + (y + r) * stride, row_bytes);
    esp_err_t err = esp_lcd_panel_io_tx_color(this->io_, cmd, this->bounce_, rows * row_bytes);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "tx_color failed: %s", esp_err_to_name(err));
      return;
    }
    this->transfer_pending_ = true;
    cmd = -1;  // continue the RAMWR data stream without re-sending the command
    y += rows;
    App.feed_wdt();
  }
  this->wait_transfer_();
}

void MipiI80::fill(Color color) {
  if (this->buffer_ == nullptr) {
    this->init_internal_(static_cast<uint32_t>(this->width_) * this->height_ * 2);
    if (this->buffer_ == nullptr) {
      this->mark_failed();
      return;
    }
  }
  uint16_t c = display::ColorUtil::color_to_565(color, display::ColorOrder::COLOR_ORDER_RGB);
  uint8_t hi = c >> 8, lo = c & 0xFF;
  size_t n = static_cast<size_t>(this->width_) * this->height_;
  uint8_t *p = this->buffer_;
  for (size_t i = 0; i < n; i++) {
    *p++ = hi;
    *p++ = lo;
  }
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_ - 1;
  this->y_high_ = this->height_ - 1;
}

void HOT MipiI80::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || y < 0 || x >= this->width_ || y >= this->height_)
    return;
  if (this->buffer_ == nullptr) {
    if (this->is_failed())
      return;
    this->init_internal_(static_cast<uint32_t>(this->width_) * this->height_ * 2);
    if (this->buffer_ == nullptr) {
      this->mark_failed();
      return;
    }
  }
  uint16_t c = display::ColorUtil::color_to_565(color, display::ColorOrder::COLOR_ORDER_RGB);
  size_t pos = (static_cast<size_t>(y) * this->width_ + x) * 2;
  uint8_t hi = c >> 8, lo = c & 0xFF;
  if (this->buffer_[pos] == hi && this->buffer_[pos + 1] == lo)
    return;
  this->buffer_[pos] = hi;
  this->buffer_[pos + 1] = lo;
  if (x < this->x_low_)
    this->x_low_ = x;
  if (y < this->y_low_)
    this->y_low_ = y;
  if (x > this->x_high_)
    this->x_high_ = x;
  if (y > this->y_high_)
    this->y_high_ = y;
}

void MipiI80::update() {
  if (this->is_failed())
    return;
  this->do_update_();
  this->flush_();
}

void MipiI80::flush_() {
  if (this->buffer_ == nullptr || this->x_high_ < this->x_low_ || this->y_high_ < this->y_low_)
    return;
  const size_t w = this->x_high_ - this->x_low_ + 1;
  const size_t h = this->y_high_ - this->y_low_ + 1;
  const size_t stride = static_cast<size_t>(this->width_) * 2;
  auto start = millis();
  this->set_addr_window_(this->x_low_, this->y_low_, this->x_high_, this->y_high_);
  this->send_rows_(this->buffer_ + this->y_low_ * stride + this->x_low_ * 2, w, h, stride);
  ESP_LOGV(TAG, "Flushed %ux%u at %u,%u in %u ms", static_cast<unsigned>(w), static_cast<unsigned>(h), this->x_low_,
           this->y_low_, static_cast<unsigned>(millis() - start));
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

// Direct path used by LVGL: write the given RGB565 big-endian block straight to the panel.
void MipiI80::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                             display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (w <= 0 || h <= 0 || this->is_failed())
    return;
  if (this->rotation_ != display::DISPLAY_ROTATION_0_DEGREES || bitness != display::COLOR_BITNESS_565 || !big_endian) {
    display::Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset,
                                     x_pad);
    return;
  }
  const size_t stride = static_cast<size_t>(x_offset + w + x_pad) * 2;
  this->set_addr_window_(x_start, y_start, x_start + w - 1, y_start + h - 1);
  this->send_rows_(ptr + y_offset * stride + x_offset * 2, w, h, stride);
}

void MipiI80::dump_config() {
  LOG_DISPLAY("", "MIPI i80 (8080) display", this);
  ESP_LOGCONFIG(TAG,
                "  Native size: %dx%d, offsets %d/%d\n"
                "  WR pin: GPIO%d, DC pin: GPIO%d, CS pin: %d\n"
                "  Data pins: %d %d %d %d %d %d %d %d\n"
                "  Pixel clock: %u MHz\n"
                "  Color order: %s, swap_xy: %s, mirror_x: %s, mirror_y: %s, invert: %s\n"
                "  Panel select strap: 0x%02X",
                this->native_width_, this->native_height_, this->offset_x_, this->offset_y_, this->wr_pin_,
                this->dc_pin_, this->cs_pin_, this->data_pins_[0], this->data_pins_[1], this->data_pins_[2],
                this->data_pins_[3], this->data_pins_[4], this->data_pins_[5], this->data_pins_[6], this->data_pins_[7],
                static_cast<unsigned>(this->pclk_hz_ / 1000000),
                this->color_order_ == display::COLOR_ORDER_BGR ? "BGR" : "RGB", YESNO(this->swap_xy_),
                YESNO(this->mirror_x_), YESNO(this->mirror_y_), YESNO(this->invert_colors_), this->panel_select_);
  LOG_PIN("  Reset pin: ", this->reset_pin_);
  if (this->is_failed())
    ESP_LOGE(TAG, "  Setup failed");
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::mipi_i80
