# Manual test plan

Run after every firmware change that touches audio, the wake word chain or the display.
One test at a time, wait for the device to settle (idle screen, no sound) between tests.
The plan applies to both boards; on the ESP32-S3-BOX-3 the boot key counts as a tap on the
screen wherever a step says to tap, and the mute key must additionally toggle the Mute switch.

Tooling: watch the log with `docker run --rm -it -v "$PWD":/config ghcr.io/esphome/esphome logs examples/agent-devkit-local.yaml --device <ip>`
(or `examples/esp32-s3-box-3-local.yaml`)
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
| B3 | Drag the volume bar on the screen to ~25 % and release | Confirmation tick at the new level, 4 segments lit; then B1 is noticeably quieter |
| B4 | Bar to 100 % | Tick is loud, top segments magenta, B1 loud without distortion |

## C. Microphone and wake word

| # | Step | Expected |
|---|---|---|
| C1 | Say "Okay Nabu" from ~1 m, normal voice | Rising two-note tone, ring turns cyan within ~0.5 s, "LISTENING", backlight full if it was dimmed. "Last wake word" sensor = Okay Nabu |
| C2 | Say "Hey Jarvis" | Same as C1, sensor = Hey Jarvis |
| C3 | Say "Okay Nabu" from ~3 m | Still detected (one or two tries are acceptable) |
| C4 | Say nothing for 10 s after C1 | Error disc for ~2 s, then idle. Wake word reacts again afterwards |

## D. Full pipeline

| # | Step | Expected |
|---|---|---|
| D1 | Wake word + "Wie spät ist es?" | Cyan → falling two-note tone when the text is recognised → amber (spinner) → magenta while the answer plays → standby. Question and answer appear in the terminal and in the Request/Response sensors |
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
| F1 | Leave the device alone for 60 s after a touch, or 30 s after an answer | Backlight dims to ~15 % |
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

## H. Timers (needs "Prefer handling commands locally" in the Assist pipeline)

| # | Step | Expected |
|---|---|---|
| H1 | "Okay Nabu, Timer eine Minute" | Assistant confirms; the ring becomes an amber arc that empties clockwise, "00:59" counts down in the centre, the disc is hidden |
| H2 | "Okay Nabu, wie lange läuft der Timer noch?" | Spoken remaining time matches the display |
| H3 | Let it run out | Backlight full, arc and "00:00" magenta, "TIMER DONE", the chime repeats every ~2 s, "Timer ringing" switch on in HA |
| H4 | Tap the screen | Chime stops at once, ring and disc are back, standby screen, switch off |
| H5 | Repeat H1 and H3, then say the wake word | Chime stops and the assistant listens |
| H6 | Repeat H1 and H3, do nothing | Chime stops by itself after two minutes |
| H7 | H1, then "Okay Nabu, Timer stoppen" | Arc and digits disappear, ring and disc are back |

## I. Language and failure feedback

| # | Step | Expected |
|---|---|---|
| I1 | In HA set "UI language" to Deutsch | Screen switches at once: BEREIT, "> DU", STUMM on the button; after a reboot it is still German |
| I2 | Set it back to English | STANDBY, "> YOU", MUTE |
| I3 | Wake word, then stay silent | After a few seconds: red FAULT (FEHLER), a low double buzz, then back to standby |

## J. Follow-up listening ("Continue conversation" switch on, "Follow-up timeout" at 8 s)

| # | Step | Expected |
|---|---|---|
| J1 | Wake word + question, then stay silent | After the answer: ring stays cyan, "LISTENING" / "NO WAKE WORD NEEDED", the thin outer ring drains and "8" counts down in the disc. At 0 the screen goes back to standby, no error tone, no red disc. Wake word works afterwards |
| J2 | Wake word + question, then ask a second question during the countdown without a wake word | Countdown disappears as soon as you speak, the second question is answered, a new window opens after that answer |
| J3 | Chain three questions this way, then stay silent | All three answered, then standby after the countdown |
| J4 | During the countdown tap the screen | Window closes at once, standby, wake word works |
| J5 | During the countdown toggle "Mute" | Window closes, MUTED screen; unmuting restarts the wake word |
| J6 | Set the timeout to 3 s and repeat J1 | Countdown runs 3, 2, 1, screen back to standby after about 3 s |
| J7 | Set the timeout to 15 s and repeat J1 | Countdown reaches 0 at the same time as Home Assistant gives up; still no error tone |
| J8 | Start a 1 minute timer, then J1 | Timer digits keep the centre, only the outer ring drains; after the window the timer continues |
| J9 | Switch "Continue conversation" off, then D1 | Old behaviour: standby right after the answer |
| J10 | With the switch on, ask something that makes the assistant ask back (D3) | Home Assistant's own follow-up runs, the countdown ring shows for it too; answering works, ignoring it ends quietly |
| J11 | Chain questions for longer than a minute without touching the screen | Backlight stays at full brightness for the whole conversation, dims 30 s after the last window closes |

Record failures with the test number, what you heard or saw, and the time, so the log can be matched.
