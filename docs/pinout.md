# Pinout: Wireless-Tag ESP32 Agent Dev Kit (AD35-S3)

GPIO map, I2C addresses, and sources. Status values:

- **schematic**: taken from the vendor schematic (rev V1.1, KiCad) and cross-checked against the
  vendor example firmware's pin header. Not yet confirmed on hardware.
- **verified**: confirmed on hardware (I2C scan, working audio, etc.) with the ESPHome version noted.

Sources:
- Vendor repo `wireless-tag-com/GTM` on GitHub: `doc/Schematic/Esp32AgentDevKit_Motherboard_Schematic_Diagram.pdf`
  (sheet "AD35-S3", rev V1.1, dated 2023-05-07) and the example firmware's board pin header
  `doc/Schematic/examples/Esp32AgentDevKit/components/QM-Y1091-4832/qmsd_board_pin.h`.
  The same repo holds the datasheet, manual, the stock firmware (`Esp32AgentDevKitFirmware_V1.2.0.bin`)
  and the board's 3D case file. Not redistributed here.
- ESP32-S3-BOX-3 reference config: https://github.com/esphome/wake-word-voice-assistants

## I2C bus (single shared bus)

| Signal | GPIO | Status | Notes |
|---|---|---|---|
| SDA | GPIO5 | verified (ESPHome 2026.9.1, I2C scan) | nets `ES_SDA`, `TP_SDA`, `AW_SDA`, `S_SDA` all map to IO5 |
| SCL | GPIO4 | verified (ESPHome 2026.9.1, I2C scan) | nets `ES_SCL`, `TP_SCL`, `AW_SCL`, `S_SCL` all map to IO4 |

ES8311, ES7210, FT6336U touch, AW9523 and the proximity sensor all share this one bus.
4.7k pull-ups to 3V3 are on the board (R6/R7 on `TP_SDA`/`TP_SCL`).

## I2S (audio)

| Signal | GPIO | Schematic net | Status | Notes |
|---|---|---|---|---|
| MCLK | GPIO47 | `ES_MCLK` | verified | shared by ES8311 and ES7210 |
| BCLK / SCLK | GPIO14 | `ES_SCLK` | verified | 100R series resistor |
| LRCLK / WS | GPIO13 | `ES_LRCK` | verified | 100R series resistor |
| DOUT (ESP -> ES8311 DSDIN) | GPIO12 | `ES_DAIN` | verified | speaker data |
| DIN (ES7210 ASDOUT -> ESP) | GPIO21 | `ES_ADOUT` | verified | mic data |

Vendor firmware runs both codecs at 16 kHz with 32-bit I2S slots and MCLK on GPIO47.
Two analog mics (`MIC1_P/N`, `MIC2_P/N`) go to the ES7210. The ES8311 DAC output (`DAC_P/N`)
feeds the HT6872 amplifier (U5) and is also looped back into the ES7210 (`AEC_P/N`, via R26/R27)
as an echo-reference channel.

## Amplifier / IO expander (AW9523)

| Function | AW9523 pin | Status | Notes |
|---|---|---|---|
| PA_CTRL (HT6872 enable, net `ES_CTRL`) | P1_5 | verified | driven high at boot and left on, like the vendor firmware |
| LCD reset (`LCD_RST`) | P1_6 | schematic | vendor firmware pulses low 10 ms at boot |
| LCD backlight (`LCD_LEDK`) | P1_0, P1_1, P1_2, P1_3, P0_0, P0_1 | schematic | six parallel LED-mode outputs, 37 mA max, duty 128/255 in vendor firmware |

AW9523 I2C address: **0x59** (vendor firmware `0xB2 >> 1`, confirmed by I2C scan and chip ID read).
AD0/AD1 are strapped with resistors on the board.

**Consequence:** no sound and no display without an AW9523 driver. ESPHome core has no AW9523
component; `agent-devkit.yaml` uses the community component `jenscski/esphome-components`
(pinned by commit) and exposes the amplifier enable as a switch. Pin numbering in that component:
0-7 = P0_0..P0_7, 8-15 = P1_0..P1_7.

## Display (ST7796U, 8-bit 8080 bus) and touch

| Signal | GPIO | Status | Notes |
|---|---|---|---|
| LCD RS / DC | GPIO45 | schematic | net `LCD1_RS` |
| LCD WR / CLK | GPIO10 | schematic | net `LCD1_WR&CLK` |
| LCD D0 | GPIO9 | schematic | |
| LCD D1 | GPIO3 | schematic | |
| LCD D2 | GPIO8 | schematic | |
| LCD D3 | GPIO18 | schematic | |
| LCD D4 | GPIO17 | schematic | |
| LCD D5 | GPIO16 | schematic | |
| LCD D6 | GPIO15 | schematic | |
| LCD D7 | GPIO7 | schematic | |
| LCD CS / RD / TE | not connected | schematic | vendor header sets them to -1 |
| LCD reset | AW9523 P1_6 | schematic | see above |
| Touch INT (FT6336U) | GPIO6 | schematic | net `TP_INT` |
| Touch RST | not connected | schematic | |

ESPHome core has no i80 driver; `components/mipi_i80/` wraps ESP-IDF's esp_lcd i80 bus. 20 MHz pixel
clock works; 40 MHz corrupts the image (ST7796 8-bit write timing). The vendor
firmware reads D0/D3 with pull-ups before bus init as a panel-revision strap (0x11 on this unit) and
picks one of two ST7796 init sequences from it; both are in `components/mipi_i80/display.py`.

## Other

| Function | GPIO | Status | Notes |
|---|---|---|---|
| BOOT button | GPIO0 | schematic | SW1, standard strapping pin |
| Native USB D-/D+ | GPIO19 / GPIO20 | schematic | rear USB-C (J7); also on mikroBUS |
| UART0 TX/RX | GPIO43 / GPIO44 | schematic | CH343P on bottom USB-C (J2), auto-reset via S8050 transistors |

mikroBUS header (datasheet): left IO1, IO2, IO42, IO41, IO40, IO39; right IO46, IO11, IO20, IO19, IO38, IO48.
Note IO19/IO20 double as native USB.

## I2C addresses

Column "scan" is filled from the `i2c: scan: true` log output.

| Device | Expected | Scan result | Notes |
|---|---|---|---|
| ES8311 (speaker DAC) | 0x18 | 0x18 | |
| ES7210 (mic ADC) | 0x40 | 0x40 | AD0/AD1 low |
| FT6336U (touch) | 0x38 | 0x38 | only after the LCD reset on AW9523 P1_6 is released; the ft63x6 driver reports chip id 100 |
| AW9523 (IO expander) | 0x59 | 0x59 | |
| Proximity sensor (MK-TP2016IRC / MK-PB2016PS) | ? | 0x39 | datasheets in vendor repo under `doc/Schematic/Sensors/` |

Scan done 2026-10-11 with ESPHome 2026.9.1 and `examples/minimal-i2c-scan.yaml`.

## Open questions

- Confirm the display and backlight entries on hardware (I2C, I2S and PA enable are done, ESPHome 2026.9.1, 2026-10-11).
- AW9523: which community ESPHome component to use, or write our own.
- ES7210: which input channels carry MIC1/MIC2 and which carries the AEC loopback
  (vendor firmware sets gain on `ES7210_INPUT_MIC3`, so channel numbering needs checking).
