#include "aw9523_output.h"
#include "esphome/core/log.h"

namespace esphome::aw9523 {

static const char *const TAG = "aw9523.output";

void AW9523Output::dump_config() {
  ESP_LOGCONFIG(TAG, "AW9523 LED output:\n  Pin: %u", this->pin_);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace esphome::aw9523
