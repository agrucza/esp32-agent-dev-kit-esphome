# ESPHome voice satellite with a HUD

ESPHome configuration that turns the Wireless-Tag **ESP32 Agent Dev Kit (AD35-S3)** or the
Espressif **ESP32-S3-BOX-3** into a fully local **Home Assistant voice satellite**: on-device
wake word, Assist pipeline, TTS playback, a HUD-style touch screen, voice timers, follow-up
listening. No vendor cloud.

Both boards carry the same audio front end (ES7210 microphone ADC, ES8311 speaker codec), so the
voice pipeline is shared. Only the hardware package and the screen layout differ. The
[esphome/wake-word-voice-assistants](https://github.com/esphome/wake-word-voice-assistants)
BOX-3 configuration is the reference for the voice parts.

## Boards

| Board | Package | Screen | Status |
|---|---|---|---|
| Wireless-Tag ESP32 Agent Dev Kit (AD35-S3) | `agent-devkit.yaml` | 320x480 portrait ST7796U, FT6336U touch | ✅ tested on hardware |
| Espressif ESP32-S3-BOX-3 | `esp32-s3-box-3.yaml` | 320x240 landscape ILI9342C, GT911 touch | ⚠️ compiles, not yet run on a BOX-3 |

## Status

| Feature | Agent Dev Kit | BOX-3 | Notes |
|---|---|---|---|
| Wi-Fi / API / OTA / logging | ✅ works | ⚠️ untested | encrypted OTA tested over Wi-Fi; dev kit logs on UART0 (bottom USB-C), BOX-3 on native USB |
| Microphone (ES7210) | ✅ works | ⚠️ untested | full duplex with the speaker, stays on during playback |
| Speaker (ES8311) | ✅ works | ⚠️ untested | no pops: duplex I2S keeps the codec clocks running; 16 kHz on both boards |
| Wake word + voice assistant | ✅ works | ⚠️ untested | on-device wake word, Assist pipeline with local STT/LLM/TTS via Home Assistant |
| Follow-up listening, timers, UI language | ✅ works | ⚠️ untested | shared package, see below |
| Display + HUD | ✅ works | ⚠️ layout unverified | dev kit: own `mipi_i80` component; BOX-3: core `mipi_spi`, layout derived from the portrait one |
| Touch | ✅ works | ⚠️ orientation unverified | dev kit: core `ft63x6`, 5-point calibration; BOX-3: core `gt911` |
| Proximity sensor (MK-PB2016PS) | ⚠️ reads, no reflection seen | – | dev kit only; presence wake disabled by default, see docs/pinout.md |
| Hardware mute key, boot key | – | ⚠️ untested | BOX-3 only; the mute latch state is mirrored into the Mute switch |

Tested with: ESPHome 2026.9.1 and Home Assistant 2026.10 (Agent Dev Kit on hardware, BOX-3
compile only, 2026-10-13).

## Use it

Add this to a new ESPHome config (the ESPHome dashboard can also import it directly from
this repository once the device runs the firmware). Pick the package for your board:

```yaml
substitutions:
  name: agent-devkit
  friendly_name: Agent Dev Kit

packages:
  # Agent Dev Kit:
  satellite: github://agrucza/esphome-voice-satellite-hud/agent-devkit.yaml@main
  # ESP32-S3-BOX-3 instead:
  # satellite: github://agrucza/esphome-voice-satellite-hud/esp32-s3-box-3.yaml@main

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

The first flash goes over USB (dev kit: bottom USB-C, BOX-3: its USB-C; both show up as
`/dev/ttyACM0`), afterwards OTA works. Use a tag instead of `@main` to pin a release. Once the
device is in Home Assistant, pick your Assist pipeline on the device page and say "Okay Nabu"
or "Hey Jarvis".

What the package gives you: the voice satellite with a HUD-style LVGL screen (state ring, the
conversation as a terminal, mute button, segmented volume bar), short signal tones on wake word,
recognised command and volume change, voice timers with a countdown in the ring and a chime loop,
a "UI language" select (English, German), a "Backlight" light that dims after a minute, "Mute" and
"Speaker amplifier" switches, "Restart", "Factory reset" and "Play test sound" buttons, a "Timer
ringing" switch, and "Request", "Response" and "Last wake word" sensors.

**Follow-up listening** (off by default): turn on the "Continue conversation" switch and the
microphone stays open after every answer, so the next question needs no wake word. The outer
ring drains over the "Follow-up timeout" (1 to 15 s, slider) with the seconds in the centre; speak
and the countdown stops, stay silent and the device goes back to standby without an error tone.
The chain continues as long as you keep talking. A tap on the screen or muting ends it. The upper
limit is Home Assistant's own 15 s no-speech timeout. When the assistant asks you a question,
Home Assistant reopens the microphone by itself regardless of the switch; with the switch on, that
turn gets the same countdown and timeout. The package carries its own copy of ESPHome's
`voice_assistant` component with two small fixes for that case (stock 2026.9.1 can cut an answer
short and reopen the microphone when the answer starts streaming while the command chime is still
playing).

## Custom wake words

The package ships with "Okay Nabu" and "Hey Jarvis". To add your own, point `micro_wake_word`
at a [micro-wake-word](https://github.com/kahrendt/microWakeWord) v2 manifest; the `.tflite`
must sit next to the `.json`. In your own config this is simply:

```yaml
micro_wake_word:
  models:
    - model: github://TaterTotterson/microWakeWords/microWakeWordsV3/hey_computer.json@main
```

Package lists merge, so the model is added to the stock ones. Raw URLs work as well, and local
paths are relative to the YAML that lists them.

Two things to know:

- **Which words listen is decided in Home Assistant**, not in the YAML. Open the device page,
  find the voice assistant's "Wake word" setting and tick the words you want. A model that is
  compiled in but not selected there stays silent.
- The standby screen names the first enabled word, so it follows that selection.

For the developer build in this repo, custom words go into `local.yaml` in the repo root, which is
git-ignored; `local.yaml.example` shows the format.

## How the packages fit together

```
agent-devkit.yaml / esp32-s3-box-3.yaml   entry per board: name, substitutions, dashboard import
packages/boards/<board>.yaml              hardware: pins, codecs, display, touch, backlight, keys
packages/core.yaml                        platform, network, API, OTA, boot sequence
packages/voice.yaml                       wake word, voice assistant, sounds, follow-up, timers
packages/hud.yaml                         fonts, draw scripts, LVGL base
packages/hud/<layout>.yaml                the LVGL page for one panel size
```

A board package provides `box_mic`, `box_speaker` (16 kHz / 16 bit on one duplex I2S bus),
`lcd`, `touch` and `backlight`; a layout provides the LVGL page with the widget ids listed at the
top of `packages/hud.yaml`. Everything else is shared. Adding a board means one hardware file and,
if its panel size is new, one layout file.

## Develop

1. Copy `secrets.yaml.example` to `secrets.yaml` and fill it in, and `local.yaml.example` to
   `local.yaml` (personal additions such as wake words, git-ignored).
2. Agent Dev Kit: back up the stock firmware once, see `docs/stock-firmware.md`.
3. Build from the checkout with the example for your board, which includes the entry package
   from the local file and the components from `components/`:
   ```bash
   docker run --rm -v "$PWD":/config --device=/dev/ttyACM0 ghcr.io/esphome/esphome \
     run examples/agent-devkit-local.yaml --device /dev/ttyACM0
   # or examples/esp32-s3-box-3-local.yaml
   ```
4. Run the manual test plan in `docs/testing.md` before tagging a release.

`examples/minimal-i2c-scan.yaml` is the bring-up config for the Agent Dev Kit from milestone 1
(Wi-Fi, API, I2C scan).

**Audio note:** microphone and speaker share one I2S bus on both boards. Stock ESPHome runs them
one at a time and restarts the codec clocks on every switch, which pops through the speaker. The
`components/i2s_audio` override adds `duplex: true`: one TX+RX channel pair that keeps running from
the first use on, so the clocks never stop, the amplifier stays on, and the microphone keeps
listening while audio plays. Both directions run at 16 kHz / 16 bit.

**Sounds:** everything in `sounds/` is 16 kHz mono FLAC because bundled files are played as they
are (only Home Assistant's TTS is transcoded). `sounds/generate.sh` recreates the synthesized tones
with ffmpeg; the test chime is the Home Assistant Voice PE timer sound, downsampled.

### Agent Dev Kit notes

See `docs/pinout.md` for the GPIO map (from the vendor schematic) and open questions.

The logger uses UART0, which the CH343P on the bottom USB-C is wired to; the ESP32-S3 default
would send logs to the native USB-JTAG port on the rear USB-C.

**OTA note:** ESPHome's safe mode only marks a new image as good after it has run for one minute
(`boot_is_good_after`). If the board is reset within that minute, for example by opening the serial
port, which toggles the CH343P auto-reset lines, the bootloader rolls back to the previous image
and the OTA looks like it never happened. Wait a minute after an OTA before touching the serial port.

### ESP32-S3-BOX-3 notes

The hardware package follows the upstream ESPHome configuration and Espressif's board support
package (`bsp/esp-box-3` in github.com/espressif/esp-bsp). Pins: I2C SDA GPIO8 / SCL GPIO18, I2S
MCLK GPIO2 / BCLK GPIO17 / LRCLK GPIO45 / DOUT GPIO15 / DIN GPIO16, amplifier enable GPIO46,
display SPI CLK GPIO7 / MOSI GPIO6 / CS GPIO5 / DC GPIO4 / reset GPIO48, backlight GPIO47, touch
interrupt GPIO3, mute latch state GPIO1, boot key GPIO0.

- The mute key engages a hardware latch that cuts the microphone. The package mirrors its state
  into the Mute switch; the software switch alone cannot release the latch.
- A short press of the boot key acts like a tap on the screen (dismiss a timer, close a follow-up
  window, wake the screen). Holding it for 10 s factory resets the device.
- The speaker runs at 16 kHz on the duplex bus (upstream uses 48 kHz); the media player resamples.
- Not yet verified on a BOX-3: touch orientation, the landscape layout, the audio path. The first
  run should walk through `docs/testing.md`.

## License

Apache-2.0. Voice/audio parts are derived from the ESP32-S3-BOX-3 configuration in
esphome/wake-word-voice-assistants (Apache-2.0).
