#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include <esp_idf_version.h>
#include <driver/i2s_std.h>

namespace esphome::i2s_audio {

class I2SAudioComponent;

class I2SAudioBase : public Parented<I2SAudioComponent> {
 public:
  void set_i2s_role(i2s_role_t role) { this->i2s_role_ = role; }
  void set_slot_mode(i2s_slot_mode_t slot_mode) { this->slot_mode_ = slot_mode; }
  void set_std_slot_mask(i2s_std_slot_mask_t std_slot_mask) { this->std_slot_mask_ = std_slot_mask; }
  void set_slot_bit_width(i2s_slot_bit_width_t slot_bit_width) { this->slot_bit_width_ = slot_bit_width; }
  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_use_apll(uint32_t use_apll) { this->use_apll_ = use_apll; }
  void set_mclk_multiple(i2s_mclk_multiple_t mclk_multiple) { this->mclk_multiple_ = mclk_multiple; }

 protected:
  i2s_role_t i2s_role_{};
  i2s_slot_mode_t slot_mode_;
  i2s_std_slot_mask_t std_slot_mask_;
  i2s_slot_bit_width_t slot_bit_width_;
  uint32_t sample_rate_;
  bool use_apll_;
  i2s_mclk_multiple_t mclk_multiple_;
};

/// Settings a duplex bus needs from each direction to build the shared channel pair.
struct I2SDuplexSlot {
  gpio_num_t data_pin{I2S_GPIO_UNUSED};
  i2s_slot_mode_t slot_mode{I2S_SLOT_MODE_MONO};
  i2s_std_slot_mask_t slot_mask{I2S_STD_SLOT_LEFT};
  i2s_slot_bit_width_t slot_bit_width{I2S_SLOT_BIT_WIDTH_16BIT};
  uint32_t sample_rate{16000};
  bool use_apll{false};
  i2s_mclk_multiple_t mclk_multiple{I2S_MCLK_MULTIPLE_256};
};

class I2SAudioIn : public I2SAudioBase {
 public:
  virtual I2SDuplexSlot get_duplex_slot() const { return {}; }
};

/// ISR-context handler for duplex TX "sent" events. Must live in IRAM and touch RAM only: the
/// I2S ISR also runs while the flash cache is disabled (OTA writes), so no virtual dispatch here.
using I2SDuplexSentFn = bool (*)(int64_t now_us, void *ctx);

class I2SAudioOut : public I2SAudioBase {
 public:
  virtual I2SDuplexSlot get_duplex_slot() const { return {}; }
};

class I2SAudioComponent final : public Component {
 public:
  i2s_std_gpio_config_t get_pin_config() const {
    return {.mclk = (gpio_num_t) this->mclk_pin_,
            .bclk = (gpio_num_t) this->bclk_pin_,
            .ws = (gpio_num_t) this->lrclk_pin_,
            .dout = I2S_GPIO_UNUSED,  // add local ports
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            }};
  }

  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }
  void set_port(int port) { this->port_ = port; }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  int get_port() const { return this->port_; }
#else
  i2s_port_t get_port() const { return static_cast<i2s_port_t>(this->port_); }
#endif

  void lock() { this->lock_.lock(); }
  bool try_lock() { return this->lock_.try_lock(); }
  void unlock() { this->lock_.unlock(); }

  // ---- full-duplex mode -------------------------------------------------------------------
  void set_duplex(bool duplex) { this->duplex_ = duplex; }
  bool is_duplex() const { return this->duplex_; }
  void set_audio_in(I2SAudioIn *in) { this->audio_in_ = in; }
  void set_audio_out(I2SAudioOut *out) { this->audio_out_ = out; }
  void set_duplex_sent_handler(I2SDuplexSentFn fn, void *ctx) {
    this->duplex_sent_fn_ = fn;
    this->duplex_sent_ctx_ = ctx;
  }
  /// Creates, initialises and enables the TX+RX pair on first use. Returns false on failure.
  bool duplex_init();
  i2s_chan_handle_t duplex_tx() const { return this->tx_handle_; }
  i2s_chan_handle_t duplex_rx() const { return this->rx_handle_; }
  /// Number of DMA descriptors the duplex channels were created with.
  uint32_t duplex_dma_desc_num() const { return this->duplex_dma_desc_num_; }
  void dump_config() override;

 protected:
  static bool duplex_tx_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);

  Mutex lock_;
  bool duplex_{false};
  bool duplex_ready_{false};
  bool duplex_failed_{false};
  uint32_t duplex_dma_desc_num_{0};
  I2SDuplexSentFn duplex_sent_fn_{nullptr};
  void *duplex_sent_ctx_{nullptr};
  i2s_chan_handle_t tx_handle_{nullptr};
  i2s_chan_handle_t rx_handle_{nullptr};

  I2SAudioIn *audio_in_{nullptr};
  I2SAudioOut *audio_out_{nullptr};
  int mclk_pin_{I2S_GPIO_UNUSED};
  int bclk_pin_{I2S_GPIO_UNUSED};
  int lrclk_pin_;
  int port_{};
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
