#include "i2s_audio.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include <esp_timer.h>

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio";

// 10 ms DMA buffers, five of them: must match the speaker's DMA_BUFFERS_COUNT so its lockstep
// bookkeeping (one write record per DMA buffer) lines up with the shared channel.
static constexpr uint32_t DUPLEX_DMA_BUFFER_MS = 10;
static constexpr uint32_t DUPLEX_DMA_DESC_NUM = 5;

void I2SAudioComponent::dump_config() {
  if (this->duplex_) {
    ESP_LOGCONFIG(TAG,
                  "I2S bus (full duplex):\n"
                  "  Port: %d\n"
                  "  MCLK: %d, BCLK: %d, LRCLK: %d\n"
                  "  Channels ready: %s",
                  this->port_, this->mclk_pin_, this->bclk_pin_, this->lrclk_pin_, YESNO(this->duplex_ready_));
  }
}

bool IRAM_ATTR I2SAudioComponent::duplex_tx_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event,
                                                    void *user_ctx) {
  auto *bus = static_cast<I2SAudioComponent *>(user_ctx);
  if (bus->duplex_sent_fn_ == nullptr)
    return false;
  return bus->duplex_sent_fn_(esp_timer_get_time(), bus->duplex_sent_ctx_);
}

bool I2SAudioComponent::duplex_init() {
  if (this->duplex_ready_)
    return true;
  if (this->duplex_failed_)
    return false;
  if (this->audio_in_ == nullptr || this->audio_out_ == nullptr) {
    ESP_LOGE(TAG, "duplex: microphone and speaker must both be registered on this bus");
    this->duplex_failed_ = true;
    return false;
  }
  const I2SDuplexSlot in = this->audio_in_->get_duplex_slot();
  const I2SDuplexSlot out = this->audio_out_->get_duplex_slot();
  if (in.sample_rate != out.sample_rate) {
    ESP_LOGE(TAG, "duplex: sample rates differ (%u vs %u)", (unsigned) in.sample_rate, (unsigned) out.sample_rate);
    this->duplex_failed_ = true;
    return false;
  }

  const uint32_t frames_per_buffer = out.sample_rate * DUPLEX_DMA_BUFFER_MS / 1000;
  i2s_chan_config_t chan_cfg = {
      .id = this->get_port(),
      .role = I2S_ROLE_MASTER,
      .dma_desc_num = DUPLEX_DMA_DESC_NUM,
      .dma_frame_num = frames_per_buffer,
      .auto_clear = true,  // DMA buffers are zeroed after being sent: silence whenever nobody writes
      .intr_priority = 3,
  };
  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, &this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "duplex: channel pair allocation failed: %s", esp_err_to_name(err));
    this->duplex_failed_ = true;
    return false;
  }
  this->duplex_dma_desc_num_ = DUPLEX_DMA_DESC_NUM;

  i2s_clock_src_t clk_src = I2S_CLK_SRC_DEFAULT;
#if SOC_CLK_APLL_SUPPORTED
  if (out.use_apll)
    clk_src = I2S_CLK_SRC_APLL;
#endif
  i2s_std_clk_config_t clk_cfg = {
      .sample_rate_hz = out.sample_rate,
      .clk_src = clk_src,
      .mclk_multiple = out.mclk_multiple,
  };

  // TX first: it becomes the clock master; RX is initialised second and turns into the
  // full-duplex slave that follows TX's bit and word clocks (ESP-IDF i2s_std.c).
  i2s_std_slot_config_t tx_slot =
      I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) out.slot_bit_width, out.slot_mode);
  tx_slot.slot_bit_width = out.slot_bit_width;
  tx_slot.slot_mask = out.slot_mask;
  i2s_std_gpio_config_t tx_gpio = this->get_pin_config();
  tx_gpio.dout = out.data_pin;
  i2s_std_config_t tx_cfg = {.clk_cfg = clk_cfg, .slot_cfg = tx_slot, .gpio_cfg = tx_gpio};

  i2s_std_slot_config_t rx_slot =
      I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) in.slot_bit_width, in.slot_mode);
  rx_slot.slot_bit_width = in.slot_bit_width;
  rx_slot.slot_mask = in.slot_mask;
  i2s_std_gpio_config_t rx_gpio = this->get_pin_config();
  rx_gpio.din = in.data_pin;
  i2s_std_config_t rx_cfg = {.clk_cfg = clk_cfg, .slot_cfg = rx_slot, .gpio_cfg = rx_gpio};

  if ((err = i2s_channel_init_std_mode(this->tx_handle_, &tx_cfg)) != ESP_OK ||
      (err = i2s_channel_init_std_mode(this->rx_handle_, &rx_cfg)) != ESP_OK) {
    ESP_LOGE(TAG, "duplex: channel init failed: %s", esp_err_to_name(err));
    this->duplex_failed_ = true;
    return false;
  }

  const i2s_event_callbacks_t callbacks = {.on_sent = duplex_tx_sent_cb};
  if ((err = i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this)) != ESP_OK) {
    ESP_LOGE(TAG, "duplex: callback registration failed: %s", esp_err_to_name(err));
    this->duplex_failed_ = true;
    return false;
  }

  if ((err = i2s_channel_enable(this->tx_handle_)) != ESP_OK || (err = i2s_channel_enable(this->rx_handle_)) != ESP_OK) {
    ESP_LOGE(TAG, "duplex: channel enable failed: %s", esp_err_to_name(err));
    this->duplex_failed_ = true;
    return false;
  }

  ESP_LOGI(TAG, "duplex: TX+RX running at %u Hz, %u frames per DMA buffer, clocks stay on",
           (unsigned) out.sample_rate, (unsigned) frames_per_buffer);
  this->duplex_ready_ = true;
  return true;
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
