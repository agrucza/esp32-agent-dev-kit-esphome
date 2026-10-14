# CLAUDE.md

Guidance for Claude (and humans) working in this repository.

## Project

ESPHome configuration that turns the **Wireless-Tag ESP32 Agent Dev Kit (model AD35-S3)** or the
**Espressif ESP32-S3-BOX-3** into a local **Home Assistant voice satellite**: on-device wake word,
Assist pipeline, TTS playback, a HUD-style LVGL screen with touch, voice timers, follow-up
listening. No vendor cloud. Both boards use the same ES7210 + ES8311 audio front end, so the
voice pipeline and the HUD are shared packages; each board adds a hardware package and a layout.

- License: **Apache-2.0**. The voice parts derive from
  [esphome/wake-word-voice-assistants](https://github.com/esphome/wake-word-voice-assistants)
  (ESP32-S3-BOX-3 config, Apache-2.0); `components/i2s_audio` and `components/voice_assistant`
  are ESPHome's own components with small changes (duplex mode; continue-conversation fixes);
  the test chime is the Home Assistant Voice PE timer sound.
- Repo language: **English** (public repo).
- Status of every feature and board: the tables in `README.md`. Keep them honest (✅ / ⚠️ / ❌).
  The BOX-3 package ran on hardware on 2026-10-13/14 (conversations, follow-up, touch, mute key,
  OTA, timers in every combination); only the boot key is still unverified there.

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
- Docker writes as root into bind mounts. The user's own compiles left `.esphome/` and
  `examples/.esphome/` root-owned; for builds run by Claude use `--user "$(id -u):$(id -g)"`
  with `ESPHOME_DATA_DIR` pointing at a user-owned copy of the cache, never create root-owned
  files in the repo.

## Package layout and the contract between the parts

```
agent-devkit.yaml, esp32-s3-box-3.yaml   entry per board: name, components_source, packages list,
                                         dashboard_import
packages/boards/<board>.yaml             hardware only: esp32/psram, logger uart, i2c, i2s, codecs,
                                         mic/speaker, amplifier switch, display, backlight, touch,
                                         board-specific sensors and keys
packages/core.yaml                       esphome:, external_components (i2s_audio, voice_assistant),
                                         api/ota/wifi/improv/captive_portal, restart + factory reset
packages/voice.yaml                      media_player + sounds, micro_wake_word, voice_assistant,
                                         mute / continue / timer_ringing switches, follow-up scripts,
                                         phase globals, 1 s convergence interval
packages/hud.yaml                        ui_language select, fonts, grid image, draw_* scripts,
                                         screen_touched, backlight dimming, LVGL base block
packages/hud/<layout>.yaml               lvgl.pages only (plus layout-specific fonts)
```

- A board package must provide the ids `box_mic`, `box_speaker` (16 kHz / 16 bit, one duplex
  I2S bus, speaker `i2s_mode: secondary`), `lcd`, `touch`, `backlight`, and set the
  `hud_title` substitution. It may call `screen_touched`, `wake_screen` (hud), `mute` (voice)
  and `factory_reset_button` (core). Nothing else in the shared packages may reference
  board hardware.
- A layout must define every widget id listed at the top of `packages/hud.yaml` (the draw
  scripts address them directly). Both panels are 320 px wide, so the top bar is identical.
- `voice.yaml` never touches LVGL directly; screen changes go through the hud scripts
  (`draw_display`, `draw_timer`, `draw_mute`, `draw_follow_up_countdown`, `wake_screen`,
  `dim_after_conversation`). Keep it that way so a board without a screen stays possible.
- File paths inside packages (`../sounds/*.flac`, `../images/grid.png`) are relative to the
  package file; ESPHome falls back to "next to the declaring document" when the config dir
  does not have them. This is what makes `github://` imports and `examples/` builds both work.
- Substitutions are global after merging: pins live in the board file, `va_phase_*` in
  `voice.yaml`, `name` / `friendly_name` / `components_source` in the entry file.
- `external_components` is listed twice on purpose (core: shared copies, board: its drivers);
  ESPHome merges the lists.

## Hardware

### Agent Dev Kit (vendor schematic rev V1.1, all pins verified on hardware)

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

### ESP32-S3-BOX-3 (upstream ESPHome config + Espressif esp-bsp `bsp/esp-box-3`; audio, display, Wi-Fi verified 2026-10-13)

| Part | Component | Notes |
|---|---|---|
| SoC | ESP32-S3 N16R8 | board `esp32s3box`, 16 MB flash, 8 MB octal PSRAM |
| Mic ADC / speaker codec | ES7210 0x40 / ES8311 0x18 | same chips as the dev kit |
| Amplifier enable | GPIO46 | strapping pin, output after boot |
| Display | ILI9342C 320x240 | core `mipi_spi` model `S3BOX`: SPI CLK GPIO7, MOSI GPIO6, CS GPIO5, DC GPIO4, reset GPIO48 (inverted), 40 MHz |
| Backlight | GPIO47 | `ledc` PWM |
| Touch | GT911 | I2C 0x5D, INT GPIO3, no transform needed (verified) |
| Keys | GPIO0 boot, GPIO1 mute | mute is a hardware latch that cuts the mic AND resets the ES7210; GPIO1 low = muted; the board file re-runs `es7210_adc.setup()` on release |
| USB | native USB-JTAG | logger `USB_SERIAL_JTAG`; auto-reset behaviour on port open not checked |
| I2C | | SDA GPIO8, SCL GPIO18 |
| I2S | | MCLK GPIO2, BCLK GPIO17, LRCLK GPIO45, DOUT GPIO15, DIN GPIO16, 16 kHz / 16 bit (upstream: 48 kHz speaker) |

## Things that are easy to break

- **Audio**: microphone and speaker share one I2S bus on both boards. Stock ESPHome tears the
  bus down on every switch and the codec pops. `duplex: true` on the bus (our
  `components/i2s_audio`) creates one TX+RX pair that never stops. Keep both at 16 kHz / 16 bit,
  speaker `i2s_mode: secondary`. The ISR path uses a plain function pointer: a virtual call
  there crashes during OTA writes.
- **Bundled sounds** must be 16 kHz mono FLAC (`sounds/generate.sh`). The media player plays
  local files as they are; only Home Assistant's TTS is transcoded. A mismatched file jams the
  announcement queue and the assistant ends up transcribing its own answers.
- **Wake word restart**: `start_wake_word` waits for the voice assistant to go idle
  (`wait_until`, `mode: restart`). A plain `if` check loses the race with the speaker stop.
- **Status screen**: event hooks arrive out of order; the 1 s interval converges the phase back
  to standby. Keep it.
- **Follow-up window** (`start_follow_up` / `end_follow_up`): stopping the pipeline from the
  device makes Home Assistant reply with `stt-no-text-recognized` and then a run end, so
  `on_error` and `on_end` both fire. `follow_up_block` tells them to end quietly and not to open
  another window; it is consumed in `on_end`. A window only opens after a run that spoke an
  answer (`follow_up_eligible`), otherwise a cancelled run and the window chase each other.
  Home Assistant's own no-speech limit is 15 s, which caps the "Follow-up timeout" slider.
  `follow_up_active` stays true after the user starts speaking into the window, until that
  run ends; "window still waiting" is `follow_up_active && phase == follow_up`. Calling
  `voice_assistant.stop` on a run that is streaming its answer cuts the answer off.
- **Vendored voice_assistant** (`components/voice_assistant`, ESPHome 2026.9.1): two fixes
  marked `AD35_FIX`. Stock ESPHome keeps the previous turn's continue flag during the next
  answer and, if the TTS URL is queued while the command chime is still announcing, ends the
  response after 2 s and restarts the microphone mid-answer. Re-apply the two marked spots when
  updating the copy for a new ESPHome release.
- **Timer chime**: `ring_timer` replays the chime itself. Do not go back to the media player's
  "repeat one": it repeats whatever played last, so a TTS answer that arrived during the chime
  took over the loop and repeated for two minutes. Announcements on the speaker media player
  **replace** the one playing, they never queue: a chime fired mid-answer cuts the answer off,
  so `ring_timer` waits for the voice assistant before every chime. `set_idle_phase` keeps the
  TIMER DONE phase while `timer_ringing` is on; a timer ending mid-run sets `follow_up_block`.
- **Dev kit display**: 20 MHz pixel clock. 40 MHz garbles the image.
- **Layouts**: a widget id missing from a layout only fails at `esphome config`; a changed
  widget type (e.g. `line` needs `->obj`, plain widgets are `lv_obj_t *`) only fails at compile.
  Validate and compile both boards after touching `hud.yaml` or a layout.

## Repo layout

```
.
├── agent-devkit.yaml           # importable entry for the Agent Dev Kit (no secrets, github:// components)
├── esp32-s3-box-3.yaml         # importable entry for the ESP32-S3-BOX-3
├── packages/
│   ├── boards/                 # agent-devkit.yaml, esp32-s3-box-3.yaml (hardware)
│   ├── core.yaml               # platform, network, API, OTA, boot
│   ├── voice.yaml              # wake word, voice assistant, sounds, follow-up, timers
│   ├── hud.yaml                # fonts, draw scripts, LVGL base
│   └── hud/                    # portrait-320x480.yaml, landscape-320x240.yaml (LVGL pages)
├── examples/
│   ├── agent-devkit-local.yaml     # developer build: entry from this checkout + local.yaml
│   ├── esp32-s3-box-3-local.yaml   # same for the BOX-3
│   └── minimal-i2c-scan.yaml       # dev kit milestone 1 bring-up config
├── local.yaml.example          # personal additions (custom wake words); copy to local.yaml, git-ignored
├── secrets.yaml.example        # copy to secrets.yaml, git-ignored
├── components/
│   ├── aw9523/                 # IO expander hub, pin schema, LED output (dev kit)
│   ├── i2s_audio/              # ESPHome's component + duplex mode (both boards)
│   ├── mipi_i80/               # 8080 display driver on esp_lcd (dev kit)
│   ├── mkpb2016ps/             # proximity sensor (dev kit)
│   └── voice_assistant/        # ESPHome's component + two "continue conversation" fixes (both)
├── sounds/                     # 16 kHz mono FLAC UI tones + generate.sh
├── images/grid.png             # HUD background tile
└── docs/                       # pinout.md (dev kit), stock-firmware.md (dev kit), testing.md
```

## Build & flash

```bash
# validate / compile (no permission needed); swap in esp32-s3-box-3-local.yaml for the BOX-3
docker run --rm -v "$PWD":/config ghcr.io/esphome/esphome config examples/agent-devkit-local.yaml
docker run --rm -v "$PWD":/config ghcr.io/esphome/esphome compile examples/agent-devkit-local.yaml

# OTA (ask first)
docker run --rm -v "$PWD":/config ghcr.io/esphome/esphome upload examples/agent-devkit-local.yaml --device 10.0.30.22

# USB (ask first; dev kit: bottom USB-C)
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
- `substitutions:` for every GPIO and I2C address; a board's pinout lives in one place, its
  board file.
- A change to `core.yaml`, `voice.yaml` or `hud.yaml` affects both boards: validate both
  examples, compile both, and test on whatever hardware is available. Say in the README which
  board a change was tested on.
- The packages must stay free of `!secret` and of local paths; users add `api`, `ota` (via
  `id: !extend ota_esphome`) and `wifi` themselves. Custom wake words go into the user's
  config, never into the packages.
- UI strings are bilingual (English, German) via the `ui_language` select; add both.
- Note the ESPHome version a change was tested on (currently 2026.9.1, Home Assistant 2026.10).
- Before a release: run `docs/testing.md`, then tag, and point the README snippet at the tag.
- Never commit `secrets.yaml`, `local.yaml`, Wi-Fi credentials, API keys, or firmware dumps.

## Target environment (for testing, not part of the config)

- Home Assistant 2026.10 at 10.0.40.30 (Proxmox VM) with an Assist pipeline using local STT
  (Wyoming faster-whisper), an OpenAI-compatible LLM conversation agent with "prefer handling
  commands locally" on (needed for timers), and local TTS (Wyoming Piper).
- The Agent Dev Kit sits on the IOT VLAN at 10.0.30.22 (pfSense static mapping), this PC on
  10.0.20.x. No mDNS across VLANs: add the device by IP. HA needs TCP 6053 to the device, the
  device needs HA's HTTP port (80 here) for TTS audio. The BOX-3 is at 10.0.30.21 (DHCP).
