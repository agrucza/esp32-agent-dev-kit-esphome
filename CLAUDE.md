# CLAUDE.md

Guidance for Claude (and humans) working in this repository.

## Project

ESPHome configuration that turns the **Wireless-Tag ESP32 Agent Dev Kit (model AD35-S3)** into a
local **Home Assistant voice satellite** (on-device wake word → Assist pipeline → TTS playback).

The stock firmware talks to a vendor cloud (aiha.cloud / Coze). The goal is a fully local device
that works like the ESP32-S3-BOX-3 voice assistant, with no vendor cloud involved.

- License: **Apache-2.0** (the audio/voice parts are derived from
  [esphome/wake-word-voice-assistants](https://github.com/esphome/wake-word-voice-assistants),
  ESP32-S3-BOX-3 config, Apache-2.0).
- Repo language: **English** (public repo).

## Hardware (from vendor datasheet + schematic, rev V1.1)

| Part | Component | Notes |
|---|---|---|
| SoC | ESP32-S3-WROOM-1 **N16R8** | 16 MB flash, 8 MB PSRAM (octal), 240 MHz dual core |
| Mic ADC | **ES7210** | 2 mics populated, I²C control + I²S data |
| Speaker codec | **ES8311** | I²C control + I²S data |
| Amplifier | **HT6872** | class-D PA, has an enable/shutdown pin (`PA_CTRL` net) |
| Display | **ST7796U**, 3.5", 320×480 | **8-bit 8080/i80 parallel bus**, not SPI |
| Touch | **FT6336U** | I²C |
| IO expander | **AW9523BTQR** | I²C; some control lines (LCD reset, possibly PA/codec) go through it |
| Proximity sensor | unnamed IR/laser sensor | I²C ("16-bit IIC, 400 kHz") |
| USB-UART | **CH343P** | on the **bottom** USB-C → use this for esptool/flashing/logs |
| Native USB | ESP32-S3 USB | **rear** USB-C (mikroBUS side) |
| Expansion | mikroBUS | IO1, IO2, IO42, IO41, IO40, IO39 (left), IO46, IO11, IO20, IO19, IO38, IO48 (right), BOOT button |

The audio front-end (ES7210 + ES8311) is the **same as the ESP32-S3-BOX-3**, which is why the
BOX-3 config is the reference.

### Unknowns, do not guess

The GPIO mapping for I²S (MCLK, SCLK/BCLK, LRCK/WS, DIN, DOUT), I²C (SDA/SCL), `PA_CTRL`,
and the LCD bus is **not yet verified**. Never invent GPIO numbers. If a pin is unknown:

- leave a clearly marked placeholder (`# TODO(pinout): unverified`), and
- record the open question in `docs/pinout.md`.

Expected I²C addresses (verify with an I²C scan before relying on them):

| Device | Typical address |
|---|---|
| ES8311 | 0x18 |
| ES7210 | 0x40 (AD0/AD1 low) |
| AW9523 | 0x58 to 0x5B (AD0/AD1 strapping) |
| FT6336U | 0x38 |

## Known ESPHome gaps

- **i80/8080 parallel display**: not supported by ESPHome core as far as we know. Treat the
  display as a stretch goal; the device must work headless first.
- **AW9523**: no core component. If the PA enable or codec control sits behind it, use an
  `external_components` driver (community or our own, kept in `components/aw9523/`).

## Milestones

0. **Back up stock firmware** (once, never commit it):
   `esptool.py --port /dev/ttyUSB0 read_flash 0 0x1000000 agent-devkit-original.bin`
1. **Minimal firmware**: Wi-Fi, API, OTA, logger, `i2c: scan: true` → confirm bus pins and addresses.
2. **Audio**: ES7210 mic + ES8311 speaker via `i2s_audio`, `audio_adc`, `audio_dac`;
   test mic levels and a test tone / media playback.
3. **Voice**: `micro_wake_word` + `voice_assistant` + `media_player`, mirroring the BOX-3 config.
4. **AW9523 / PA enable** if milestone 2 has no sound.
5. **Display** (stretch): status screens like the BOX-3 (idle/listening/thinking/replying/error).
6. **Distribution**: `packages:` + `dashboard_import`, tagged releases, entry on devices.esphome.io.

## Repo layout

```
.
├── CLAUDE.md
├── README.md              # status table: what works / what doesn't
├── LICENSE                # Apache-2.0
├── agent-devkit.yaml      # main config (importable via packages)
├── examples/
│   └── minimal-i2c-scan.yaml
├── components/            # external_components (only if needed)
├── docs/
│   └── pinout.md          # verified GPIO map, I²C addresses, sources
├── secrets.yaml.example
└── .gitignore             # must contain secrets.yaml, .esphome/, *.bin
```

## Build & flash

ESPHome via Docker (no local install needed):

```bash
# compile
docker run --rm -v "$PWD":/config ghcr.io/esphome/esphome compile agent-devkit.yaml

# first flash over USB (bottom USB-C, CH343P); adjust the device path
docker run --rm -v "$PWD":/config --device=/dev/ttyUSB0 ghcr.io/esphome/esphome \
  run agent-devkit.yaml --device /dev/ttyUSB0

# logs
docker run --rm -v "$PWD":/config --device=/dev/ttyUSB0 ghcr.io/esphome/esphome \
  logs agent-devkit.yaml --device /dev/ttyUSB0
```

After the first flash, OTA via `--device <ip>` works without USB.
If the port does not show up: check `dmesg -w` while plugging in, the `dialout` group, and the cable
(must carry data); hold BOOT while plugging in to force download mode.

## Conventions

- **Header** in every file derived from upstream:
  ```yaml
  # Based on the ESP32-S3-BOX-3 configuration from
  # https://github.com/esphome/wake-word-voice-assistants (Apache-2.0)
  # Modified for the Wireless-Tag ESP32 Agent Dev Kit (AD35-S3)
  ```
- Use `substitutions:` for every GPIO and I²C address so the pinout lives in one place.
- Pin ESPHome-relevant versions in docs (`ESPHome 2026.9.x` at time of writing); note the version
  each milestone was tested on.
- Never commit `secrets.yaml`, Wi-Fi credentials, API keys, or the stock firmware dump.
- Keep the README status table honest (✅ works / ⚠️ partial / ❌ not supported).
- Small, focused commits; one milestone per PR/branch is fine.

## Target environment (for testing, not part of the config)

- Home Assistant 2026.10 with an Assist pipeline using local STT (Wyoming faster-whisper),
  an OpenAI-compatible LLM conversation agent, and local TTS (Wyoming Piper).
- The satellite may sit in a different VLAN than Home Assistant: no mDNS discovery, add by IP,
  and HA must reach the device on TCP 6053 while the device must reach HA's HTTP port to fetch TTS audio.
