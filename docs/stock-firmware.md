# Stock firmware backup

The vendor firmware was dumped once before flashing ESPHome. The dump is **not** in this repo
(`*.bin` is git-ignored); keep it somewhere safe.

| | |
|---|---|
| File | `agent-devkit-original.bin` (16 777 216 bytes, full 16 MB flash) |
| SHA-256 | `7c4627a080b86ab1248046738f7d9f807643d3448b4462a12856b70c3f42875c` |
| Dumped on | 2026-10-11, esptool 5.x (pip), 921600 baud, `/dev/ttyACM0` |
| Chip | ESP32-S3 (QFN56) rev v0.2, embedded 8 MB PSRAM, 16 MB quad flash, 40 MHz crystal |
| Stock app | project `ad35`, version `bc5d881-dirty`, built 2024-12-31, ESP-IDF v5.3.2 |

The vendor also publishes an image `doc/Esp32AgentDevKitFirmware_V1.2.0.bin` in the
`wireless-tag-com/GTM` GitHub repo, which can serve as a second fallback.

## Dump procedure

The bottom USB-C (CH343P) enumerates as `/dev/ttyACM0` on Linux. The Debian `esptool` package
(4.7) lacks the ESP32-S3 stub flasher, so use a pip-installed esptool (or the one inside the ESPHome
Docker image):

```bash
python3 -m venv ~/esptool-venv && ~/esptool-venv/bin/pip install esptool
~/esptool-venv/bin/esptool --port /dev/ttyACM0 --baud 921600 flash_id
~/esptool-venv/bin/esptool --port /dev/ttyACM0 --baud 921600 read_flash 0 0x1000000 agent-devkit-original.bin
sha256sum agent-devkit-original.bin
```

At 921600 baud the full read takes about 4 minutes.

## Restore

```bash
~/esptool-venv/bin/esptool --port /dev/ttyACM0 --baud 921600 write_flash 0 agent-devkit-original.bin
```

## Stock partition table

| Name | Type | SubType | Offset | Size |
|---|---|---|---|---|
| nvs | data | nvs | 0x9000 | 0x6000 (24 KB) |
| otadata | data | ota | 0xF000 | 0x2000 (8 KB) |
| phy_init | data | phy | 0x11000 | 0x1000 (4 KB) |
| model | data | spiffs | 0x12000 | 0x7D000 (500 KB) |
| res | data | littlefs | 0x8F000 | 0x3C8000 (3872 KB) |
| font | data | undefined | 0x457000 | 0x1A9000 (1700 KB) |
| ota_0 | app | ota_0 | 0x600000 | 0x500000 (5120 KB) |
| ota_1 | app | ota_1 | 0xB00000 | 0x500000 (5120 KB) |

Only `ota_0` holds an app; `ota_1` is blank. `model` is most likely the Espressif wake-word /
speech model partition, `res` and `font` hold UI assets.
