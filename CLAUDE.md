# CLAUDE.md

Guidance for Claude (and humans) working in this repository.

## Project

ESPHome configuration that turns the **Wireless-Tag ESP32 Agent Dev Kit (model AD35-S3)** into a
local **Home Assistant voice satellite**: on-device wake word, Assist pipeline, TTS playback, a
HUD-style LVGL screen with touch, voice timers. No vendor cloud.

- License: **Apache-2.0**. The voice parts derive from
  [esphome/wake-word-voice-assistants](https://github.com/esphome/wake-word-voice-assistants)
  (ESP32-S3-BOX-3 config, Apache-2.0); `components/i2s_audio` is ESPHome's own component with a
  duplex mode added; the test chime is the Home Assistant Voice PE timer sound.
- Repo language: **English** (public repo).
- Status of every feature: the table in `README.md`. Keep it honest (✅ / ⚠️ / ❌).

## Rules for working on the device

- **Never flash the device without asking first.** OTA or USB, every time, one yes per flash.
  Compiling and `esphome config` need no permission. State what the build contains before asking.
- **A reset within one minute after an OTA rolls the OTA back.** ESPHome marks an image good
  after `boot_is_good_after` (1 min). Opening `/dev/ttyACM0` toggles the CH343P auto-reset lines
  and resets the board. Verify a running build over the API (`device_info().compilation_time`),
  not over serial. A reboot for a serial boot log is also something to ask for.
- Every API client connection runs `api.on_client_connected`, which restarts the wake word
  engine and redraws the screen. Log sessions over the API therefore "heal" a stuck state; do
  not mistake that for the bug being gone.
- Logs: `docker run --rm -it ... logs examples/agent-devkit-local.yaml --device 10.0.30.22`
  for a human. For automated capture read `/dev/ttyACM0` with pyserial (set DTR and RTS low
  before opening) rather than `docker logs`, whose output is block-buffered.

## Hardware (vendor schematic rev V1.1, all pins verified on hardware)

| Part | Component | Notes |
|---|---|---|
| SoC | ESP32-S3-WROOM-1 N16R8 | 16 MB flash, 8 MB octal PSRAM |
| Mic ADC | ES7210 | I2C 0x40, two analog mics on the flex, plus a DAC loopback (unused, no AEC in ESPHome) |
| Speaker codec | ES8311 | I2C 0x18 |
| Amplifier | HT6872 | enable on AW9523 P1_5, kept on permanently like the vendor firmware |
| IO expander | AW9523B | I2C 0x59; amplifier enable P1_5, LCD/touch reset P1_6, backlight P1_0..P1_3 + P0_0, P0_1 in LED mode |
| Display | ST7796U 320x480 | 8-bit 8080 bus: D0..D7 = GPIO 9, 3, 8, 18, 17, 16, 15, 7; WR GPIO10; RS/DC GPIO45; 20 MHz max |
| Touch | FT6336U | I2C 0x38, INT GPIO6, reset shared with the LCD |
| Proximity | MK-PB2016PS-A01E | I2C 0x39, IR emitter on the mic flex; reads but sees no reflection on the dev unit |
| USB-UART | CH343P | bottom USB-C, `/dev/ttyACM0`, UART0 = GPIO43/44; the logger must use UART0 |
| Native USB | ESP32-S3 | rear USB-C, unused |
| I2C | | SDA GPIO5, SCL GPIO4, one bus for everything |
| I2S | | MCLK GPIO47, BCLK GPIO14, LRCLK GPIO13, DOUT GPIO12, DIN GPIO21, 16 kHz / 16 bit |

Details and sources: `docs/pinout.md`. Stock firmware backup and partition table:
`docs/stock-firmware.md`. Vendor material (schematics, datasheets, example firmware):
github.com/wireless-tag-com/GTM.

## Things that are easy to break

- **Audio**: microphone and speaker share one I2S bus. Stock ESPHome tears the bus down on every
  switch and the codec pops. `duplex: true` on the bus (our `components/i2s_audio`) creates one
  TX+RX pair that never stops. Keep both at 16 kHz / 16 bit, speaker `i2s_mode: secondary`.
  The ISR path uses a plain function pointer: a virtual call there crashes during OTA writes.
- **Bundled sounds** must be 16 kHz mono FLAC (`sounds/generate.sh`). The media player plays
  local files as they are; only Home Assistant's TTS is transcoded. A mismatched file jams the
  announcement queue and the assistant ends up transcribing its own answers.
- **Wake word restart**: `start_wake_word` waits for the voice assistant to go idle
  (`wait_until`, `mode: restart`). A plain `if` check loses the race with the speaker stop.
- **Status screen**: event hooks arrive out of order; the 1 s interval converges the phase back
  to standby. Keep it.
- **Display**: 20 MHz pixel clock. 40 MHz garbles the image.

## Repo layout

```
.
├── agent-devkit.yaml           # the importable package (no secrets, github:// components)
├── examples/
│   ├── agent-devkit-local.yaml # developer build: package from this checkout + local.yaml
│   └── minimal-i2c-scan.yaml   # milestone 1 bring-up config
├── local.yaml.example          # personal additions (custom wake words); copy to local.yaml, git-ignored
├── secrets.yaml.example        # copy to secrets.yaml, git-ignored
├── components/
│   ├── aw9523/                 # IO expander hub, pin schema, LED output
│   ├── i2s_audio/              # ESPHome's component + duplex mode
│   ├── mipi_i80/               # 8080 display driver on esp_lcd
│   └── mkpb2016ps/             # proximity sensor
├── sounds/                     # 16 kHz mono FLAC UI tones + generate.sh
├── images/grid.png             # HUD background tile
└── docs/                       # pinout.md, stock-firmware.md, testing.md (manual test plan)
```

## Build & flash

```bash
# compile (no permission needed)
docker run --rm -v "$PWD":/config ghcr.io/esphome/esphome compile examples/agent-devkit-local.yaml

# OTA (ask first)
docker run --rm -v "$PWD":/config ghcr.io/esphome/esphome upload examples/agent-devkit-local.yaml --device 10.0.30.22

# USB (ask first; bottom USB-C)
docker run --rm -v "$PWD":/config --device=/dev/ttyACM0 ghcr.io/esphome/esphome \
  run examples/agent-devkit-local.yaml --device /dev/ttyACM0
```

The Debian `esptool` package lacks the ESP32-S3 stub; use a pip-installed esptool for raw
flash access (`docs/stock-firmware.md`).

## Conventions

- Header in every file derived from upstream:
  ```yaml
  # Based on the ESP32-S3-BOX-3 configuration from
  # https://github.com/esphome/wake-word-voice-assistants (Apache-2.0)
  # Modified for the Wireless-Tag ESP32 Agent Dev Kit (AD35-S3)
  ```
- `substitutions:` for every GPIO and I2C address; the pinout lives in one place.
- The package must stay free of `!secret` and of local paths; users add `api`, `ota` (via
  `id: !extend ota_esphome`) and `wifi` themselves. Custom wake words go into the user's
  config, never into the package.
- UI strings are bilingual (English, German) via the `ui_language` select; add both.
- Note the ESPHome version a change was tested on (currently 2026.9.1, Home Assistant 2026.10).
- Before a release: run `docs/testing.md`, then tag, and point the README snippet at the tag.
- Never commit `secrets.yaml`, `local.yaml`, Wi-Fi credentials, API keys, or firmware dumps.

## Target environment (for testing, not part of the config)

- Home Assistant 2026.10 at 10.0.40.30 (Proxmox VM) with an Assist pipeline using local STT
  (Wyoming faster-whisper), an OpenAI-compatible LLM conversation agent with "prefer handling
  commands locally" on (needed for timers), and local TTS (Wyoming Piper).
- The device sits on the IOT VLAN at 10.0.30.22 (pfSense static mapping), this PC on
  10.0.20.x. No mDNS across VLANs: add the device by IP. HA needs TCP 6053 to the device, the
  device needs HA's HTTP port (80 here) for TTS audio.
