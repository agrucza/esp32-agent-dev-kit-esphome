#!/bin/sh
# Regenerates the synthesized UI sounds (16 kHz mono FLAC, the format the device plays as-is).
# wake.flac:    two-note rise, played when a wake word is detected
# command.flac: two-note fall, played when the command was recognised and after a volume change
# timer_finished.flac is the Home Assistant Voice PE chime (Apache-2.0), downsampled, not generated here.
set -e
cd "$(dirname "$0")"
FMT="aresample=16000,aformat=sample_fmts=s16:channel_layouts=mono"
ffmpeg -loglevel error -y \
  -f lavfi -i "sine=frequency=740:duration=0.09,afade=t=in:d=0.01,afade=t=out:st=0.06:d=0.03,volume=0.35" \
  -f lavfi -i "sine=frequency=1109:duration=0.12,afade=t=in:d=0.01,afade=t=out:st=0.07:d=0.05,volume=0.35" \
  -filter_complex "[0:a][1:a]concat=n=2:v=0:a=1,$FMT" -ar 16000 -ac 1 -sample_fmt s16 wake.flac
ffmpeg -loglevel error -y \
  -f lavfi -i "sine=frequency=988:duration=0.09,afade=t=in:d=0.005,afade=t=out:st=0.06:d=0.03,volume=0.55" \
  -f lavfi -i "sine=frequency=659:duration=0.11,afade=t=in:d=0.005,afade=t=out:st=0.06:d=0.05,volume=0.55" \
  -filter_complex "[0:a][1:a]concat=n=2:v=0:a=1,$FMT" -ar 16000 -ac 1 -sample_fmt s16 command.flac
for f in *.flac; do printf '%s: ' "$f"; ffprobe -v error -show_entries stream=sample_rate,channels,duration -of csv=p=0 "$f"; done
