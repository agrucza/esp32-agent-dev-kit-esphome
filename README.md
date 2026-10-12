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
| Display (ST7796U, 8-bit 8080 bus) | ✅ works | own `mipi_i80` component on ESP-IDF esp_lcd; LVGL HUD with mute and volume |
| Touch (FT6336U) | ✅ works | core `ft63x6`, 5-point calibration in the config |

Tested with: ESPHome 2026.9.1 and Home Assistant 2026.10 (milestones 1 to 5 on hardware, 2026-10-11).

## Use it

Add this to a new ESPHome config (the ESPHome dashboard can also import it directly from
this repository once the device runs the firmware):

```yaml
substitutions:
  name: agent-devkit
  friendly_name: Agent Dev Kit

packages:
  agent_devkit: github://agrucza/esp32-agent-dev-kit-esphome/agent-devkit.yaml@main

esphome:
  name: ${name}
  name_add_mac_suffix: false

api:
  encryption:
    key: !secret api_encryption_key

ota:
  - id: !extend ota_esphome
    encryption:
      key: !secret api_encryption_key

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

The first flash goes over the bottom USB-C (shows up as `/dev/ttyACM0`), afterwards OTA works.
Use a tag instead of `@main` to pin a release. Once the device is in Home Assistant, pick your
Assist pipeline on the device page and say "Okay Nabu" or "Hey Jarvis".

What the package gives you: the voice satellite with a HUD-style LVGL screen (state ring, the
conversation as a terminal, mute button, segmented volume bar), short signal tones on wake word,
recognised command and volume change, voice timers with a countdown in the ring and a chime loop, a "UI language" select (English, German), a "Backlight" light that dims after a minute, "Mute" and
"Speaker amplifier" switches, "Restart", "Factory reset" and "Play test sound" buttons, a "Timer ringing" switch, and "Request",
"Response" and "Last wake word" sensors.

## Develop

1. Copy `secrets.yaml.example` to `secrets.yaml` and fill it in.
2. Back up the stock firmware once, see `docs/stock-firmware.md`.
3. Build from the checkout with `examples/agent-devkit-local.yaml`, which includes the package
   from the local file and the components from `components/`:
   ```bash
   docker run --rm -v "$PWD":/config --device=/dev/ttyACM0 ghcr.io/esphome/esphome \
     run examples/agent-devkit-local.yaml --device /dev/ttyACM0
   ```
4. Run the manual test plan in `docs/testing.md` before tagging a release.

`examples/minimal-i2c-scan.yaml` is the bring-up config from milestone 1 (Wi-Fi, API, I2C scan).

See `docs/pinout.md` for the GPIO map (from the vendor schematic) and open questions.

**Audio note:** microphone and speaker share one I2S bus. Stock ESPHome runs them one at a time
and restarts the codec clocks on every switch, which pops through the speaker on this board. The
`components/i2s_audio` override adds `duplex: true`: one TX+RX channel pair that keeps running from
the first use on, so the clocks never stop, the amplifier stays on, and the microphone keeps
listening while audio plays. Both directions run at 16 kHz / 16 bit.

**Sounds:** everything in `sounds/` is 16 kHz mono FLAC because bundled files are played as they
are (only Home Assistant's TTS is transcoded). `sounds/generate.sh` recreates the synthesized tones
with ffmpeg; the test chime is the Home Assistant Voice PE timer sound, downsampled.

**OTA note:** ESPHome's safe mode only marks a new image as good after it has run for one minute
(`boot_is_good_after`). If the board is reset within that minute, for example by opening the serial
port, which toggles the CH343P auto-reset lines, the bootloader rolls back to the previous image
and the OTA looks like it never happened. Wait a minute after an OTA before touching the serial port.

## License

Apache-2.0. Voice/audio parts are derived from the ESP32-S3-BOX-3 configuration in
esphome/wake-word-voice-assistants (Apache-2.0).
