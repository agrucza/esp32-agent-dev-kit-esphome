# Manual test plan

Run after every firmware change that touches audio, the wake word chain or the display.
One test at a time, wait for the device to settle (idle screen, no sound) between tests.

Tooling: watch the log with `docker run --rm -it -v "$PWD":/config ghcr.io/esphome/esphome logs agent-devkit.yaml --device <ip>`
(over Wi-Fi, so the serial auto-reset cannot interfere). Entity states are visible on the device page in Home Assistant.

## A. Boot

| # | Step | Expected |
|---|---|---|
| A1 | Power cycle the board | Screen shows the idle page within ~3 s, backlight at full brightness, no sound at all during boot |
| A2 | Wait for Wi-Fi and HA | Green dot top right, label "Say "Okay Nabu"". Without HA: red dot, "Not connected" |

## B. Audio output (speaker path)

| # | Step | Expected |
|---|---|---|
| B1 | Press "Play test sound" in HA | Chime plays, clean start, no pop before or after it |
| B2 | Press it again immediately after it finished | Same, no pop, no "Parent bus is busy" in the log |
| B3 | Move the volume slider on the screen to ~25 %, then B1 | Noticeably quieter chime, slider stays where released |
| B4 | Slider to 100 %, then B1 | Loud, no distortion |

## C. Microphone and wake word

| # | Step | Expected |
|---|---|---|
| C1 | Say "Okay Nabu" from ~1 m, normal voice | Disc turns blue within ~0.5 s, "Listening", backlight full if it was dimmed. "Last wake word" sensor = Okay Nabu |
| C2 | Say "Hey Jarvis" | Same as C1, sensor = Hey Jarvis |
| C3 | Say "Okay Nabu" from ~3 m | Still detected (one or two tries are acceptable) |
| C4 | Say nothing for 10 s after C1 | Error disc for ~2 s, then idle. Wake word reacts again afterwards |

## D. Full pipeline

| # | Step | Expected |
|---|---|---|
| D1 | Wake word + "Wie spät ist es?" | Blue → orange (spinner) → green while the answer plays → idle. Question and answer appear on the screen and in the Request/Response sensors |
| D2 | Repeat D1 three times with ~10 s pauses | Every time the wake word is picked up again, no restart needed |
| D3 | Ask something that makes the agent ask back, then answer the follow-up | Second turn works without a wake word; after the final answer the screen returns to idle |
| D4 | Same as D3 but ignore the follow-up | Error disc briefly, then idle, wake word works again |
| D5 | Ask a question with a long answer (3+ sentences) | Text wraps, nothing overflows the bottom bar, playback not cut off |
| D6 | Say the wake word while an answer is still playing | The answer is interrupted and a new request starts (microphone stays on during playback); idle afterwards |

## E. Mute

| # | Step | Expected |
|---|---|---|
| E1 | Tap the mute button on the screen | Button turns red, label "Muted", "Mute" switch on in HA, no pop |
| E2 | Say "Okay Nabu" while muted | No reaction |
| E3 | Tap mute again | Button grey, label "Say "Okay Nabu"", no pop, wake word works again |
| E4 | Toggle the "Mute" switch from HA | Button on the screen follows |

## F. Display and touch

| # | Step | Expected |
|---|---|---|
| F1 | Leave the device alone for 60 s | Backlight dims to ~15 % |
| F2 | Tap anywhere | Backlight back to full within ~0.3 s |
| F3 | Dim it again, then say the wake word | Backlight back to full, pipeline runs |
| F4 | Tap the slider knob and drag it slowly across | Knob follows the finger over the whole width, no jumps |
| F5 | Tap the mute button at its very edge | Still toggles (touch calibration) |

## G. Pops and clicks (listen with the room quiet)

| # | Step | Expected |
|---|---|---|
| G1 | Boot | Silent |
| G2 | Mute / unmute | Silent |
| G3 | Start of the chime (B1) | Clean |
| G4 | End of an answer, when the wake word restarts | Silent |
| G5 | Volume change via slider while idle | Silent |

Record failures with the test number, what you heard or saw, and the time, so the log can be matched.
