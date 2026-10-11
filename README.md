# ESPHome for the Wireless-Tag ESP32 Agent Dev Kit (AD35-S3)

ESPHome configuration that turns the Wireless-Tag **ESP32 Agent Dev Kit** into a fully local
**Home Assistant voice satellite**: on-device wake word, Assist pipeline, TTS playback.
No vendor cloud.

The audio front-end (ES7210 mic ADC + ES8311 speaker codec) matches the ESP32-S3-BOX-3, so the
[esphome/wake-word-voice-assistants](https://github.com/esphome/wake-word-voice-assistants)
BOX-3 configuration is the reference.

## Status

| Feature | Status | Notes |
|---|---|---|
| Wi-Fi / API / OTA / logging | ✅ works | encrypted OTA tested over Wi-Fi; logs on UART0 (bottom USB-C) |
| I2C bus + device addresses verified | ✅ GPIO5/GPIO4, found 0x18, 0x39, 0x40, 0x59 | touch (0x38) missing, see docs/pinout.md |
| Microphone (ES7210) | ✅ works | full duplex with the speaker, stays on during playback |
| Speaker (ES8311 + HT6872) | ✅ works | no pops: duplex I2S keeps the codec clocks running |
| Wake word + voice assistant | ✅ works | on-device wake word, Assist pipeline with local STT/LLM/TTS via Home Assistant |
| Display (ST7796U, 8-bit 8080 bus) | ✅ works | own `mipi_i80` component on ESP-IDF esp_lcd; LVGL status page with mute and volume |
| Touch (FT6336U) | ✅ works | core `ft63x6`, 5-point calibration in the config |

Tested with: ESPHome 2026.9.1 and Home Assistant 2026.10 (milestones 1 to 5 on hardware, 2026-10-11).

## Getting started

1. Copy `secrets.yaml.example` to `secrets.yaml` and fill in your values.
2. Back up the stock firmware once (bottom USB-C, CH343P, shows up as `/dev/ttyACM0`).
   See `docs/stock-firmware.md` for the full procedure and restore command:
   ```bash
   esptool --port /dev/ttyACM0 --baud 921600 read_flash 0 0x1000000 agent-devkit-original.bin
   ```
3. Compile and flash. Start with the minimal I2C scan config, then use `agent-devkit.yaml`:
   ```bash
   docker run --rm -v "$PWD":/config --device=/dev/ttyACM0 ghcr.io/esphome/esphome \
     run examples/minimal-i2c-scan.yaml --device /dev/ttyACM0
   ```

See `docs/pinout.md` for the GPIO map (from the vendor schematic) and open questions.

**Audio note:** microphone and speaker share one I2S bus. Stock ESPHome runs them one at a time
and restarts the codec clocks on every switch, which pops through the speaker on this board. The
`components/i2s_audio` override adds `duplex: true`: one TX+RX channel pair that keeps running from
the first use on, so the clocks never stop, the amplifier stays on, and the microphone keeps
listening while audio plays. Both directions run at 16 kHz / 16 bit.

**OTA note:** ESPHome's safe mode only marks a new image as good after it has run for one minute
(`boot_is_good_after`). If the board is reset within that minute, for example by opening the serial
port, which toggles the CH343P auto-reset lines, the bootloader rolls back to the previous image
and the OTA looks like it never happened. Wait a minute after an OTA before touching the serial port.

## License

Apache-2.0. Voice/audio parts are derived from the ESP32-S3-BOX-3 configuration in
esphome/wake-word-voice-assistants (Apache-2.0).
