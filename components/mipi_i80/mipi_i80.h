// MIPI DBI Type B (8080 / i80) parallel display driver for ESPHome, built on ESP-IDF's esp_lcd i80 bus.
// Written for the ST7796U panel of the Wireless-Tag ESP32 Agent Dev Kit (AD35-S3), but generic enough
// for other 8-bit 8080 panels that use the usual MIPI DCS command set.
// Structure follows esphome/components/ili9xxx (Apache-2.0 / ESPHome license).
#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/display_color_utils.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_io_i80.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <vector>

namespace esphome::mipi_i80 {

static const char *const TAG = "mipi_i80";

// Special value in an init sequence: [cmd, DELAY_FLAG] => delay cmd ms
static constexpr uint8_t DELAY_FLAG = 0xFF;

// MIPI DCS commands used here
static constexpr uint8_t CMD_INVOFF = 0x20;
static constexpr uint8_t CMD_INVON = 0x21;
static constexpr uint8_t CMD_CASET = 0x2A;
static constexpr uint8_t CMD_PASET = 0x2B;
static constexpr uint8_t CMD_RAMWR = 0x2C;
static constexpr uint8_t CMD_MADCTL = 0x36;

static constexpr uint8_t MADCTL_MY = 0x80;
static constexpr uint8_t MADCTL_MX = 0x40;
static constexpr uint8_t MADCTL_MV = 0x20;
static constexpr uint8_t MADCTL_BGR = 0x08;

// Number of display lines moved per DMA transaction. The bounce buffer lives in internal DMA-capable RAM.
static constexpr size_t BOUNCE_LINES = 16;

class MipiI80 : public display::DisplayBuffer {
 public:
  void set_data_pins(std::vector<int8_t> pins) { this->data_pins_ = std::move(pins); }
  void set_wr_pin(int8_t pin) { this->wr_pin_ = pin; }
  void set_dc_pin(int8_t pin) { this->dc_pin_ = pin; }
  void set_cs_pin(int8_t pin) { this->cs_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_pclk_frequency(uint32_t hz) { this->pclk_hz_ = hz; }
  void set_dimensions(int16_t width, int16_t height) {
    this->native_width_ = width;
    this->native_height_ = height;
  }
  void set_offsets(int16_t x, int16_t y) {
    this->offset_x_ = x;
    this->offset_y_ = y;
  }
  void set_init_sequence(const std::vector<uint8_t> &seq) { this->init_sequence_ = seq; }
  void set_alt_init_sequence(const std::vector<uint8_t> &seq) { this->alt_init_sequence_ = seq; }
  // Read two data lines as inputs before the bus is set up; the vendor firmware selects the init
  // sequence from this strap value (0x11 => primary, anything else => alternate).
  void set_panel_select_pins(int8_t high_nibble_pin, int8_t low_nibble_pin) {
    this->sel_hi_pin_ = high_nibble_pin;
    this->sel_lo_pin_ = low_nibble_pin;
  }
  void set_invert_colors(bool invert) { this->invert_colors_ = invert; }
  void set_color_order(display::ColorOrder order) { this->color_order_ = order; }
  void set_swap_xy(bool v) { this->swap_xy_ = v; }
  void set_mirror_x(bool v) { this->mirror_x_ = v; }
  void set_mirror_y(bool v) { this->mirror_y_ = v; }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void fill(Color color) override;
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;

  // Called from the esp_lcd ISR when a color transfer finished.
  bool on_color_trans_done_();

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }

  bool init_bus_();
  void run_init_sequence_(const std::vector<uint8_t> &seq);
  void write_command_(uint8_t cmd, const uint8_t *data = nullptr, size_t len = 0);
  void write_command_(uint8_t cmd, uint8_t data) { this->write_command_(cmd, &data, 1); }
  void set_madctl_();
  void set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
  // Stream `h` rows of `w` pixels (16 bit, big endian) starting at `src` with `stride` bytes per row.
  void send_rows_(const uint8_t *src, size_t w, size_t h, size_t stride);
  void wait_transfer_();
  void flush_();

  std::vector<int8_t> data_pins_{};
  int8_t wr_pin_{-1};
  int8_t dc_pin_{-1};
  int8_t cs_pin_{-1};
  int8_t sel_hi_pin_{-1};
  int8_t sel_lo_pin_{-1};
  GPIOPin *reset_pin_{nullptr};
  uint32_t pclk_hz_{40000000};

  int16_t native_width_{320};
  int16_t native_height_{480};
  int16_t width_{320};   // after swap_xy
  int16_t height_{480};  // after swap_xy
  int16_t offset_x_{0};
  int16_t offset_y_{0};

  std::vector<uint8_t> init_sequence_{};
  std::vector<uint8_t> alt_init_sequence_{};
  int panel_select_{-1};

  bool invert_colors_{false};
  display::ColorOrder color_order_{display::COLOR_ORDER_BGR};
  bool swap_xy_{false};
  bool mirror_x_{false};
  bool mirror_y_{false};

  // dirty window watermarks (display coordinates)
  uint16_t x_low_{0}, y_low_{0}, x_high_{0}, y_high_{0};

  esp_lcd_i80_bus_handle_t bus_{nullptr};
  esp_lcd_panel_io_handle_t io_{nullptr};
  uint8_t *bounce_{nullptr};
  size_t bounce_size_{0};
  SemaphoreHandle_t done_{nullptr};
  bool transfer_pending_{false};
};

}  // namespace esphome::mipi_i80
